// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/message_filter.h"
#include "media/gpu/ipc/common/media_messages.h"
#include "ui/gfx/geometry/size.h"

namespace {

void DecodeFinished(std::unique_ptr<base::SharedMemory> shm) {
  // Do nothing. Because VideoFrame is backed by |shm|, the purpose of this
  // function is to just keep reference of |shm| to make sure it lives until
  // decode finishes.
}

bool VerifyDecodeParams(const AcceleratedJpegDecoderMsg_Decode_Params& params) {
  const int kJpegMaxDimension = UINT16_MAX;
  if (params.coded_size.IsEmpty() ||
      params.coded_size.width() > kJpegMaxDimension ||
      params.coded_size.height() > kJpegMaxDimension) {
    LOG(ERROR) << "invalid coded_size " << params.coded_size.ToString();
    return false;
  }

  if (!base::SharedMemory::IsHandleValid(params.output_video_frame_handle)) {
    LOG(ERROR) << "invalid output_video_frame_handle";
    return false;
  }

  if (params.output_buffer_size <
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420,
                                        params.coded_size)) {
    LOG(ERROR) << "output_buffer_size is too small: "
               << params.output_buffer_size;
    return false;
  }

  return true;
}

}  // namespace

namespace media {

class GpuJpegDecodeAccelerator::Client : public JpegDecodeAccelerator::Client {
 public:
  Client(base::WeakPtr<GpuJpegDecodeAccelerator> owner,
         int32_t route_id,
         scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : owner_(std::move(owner)),
        route_id_(route_id),
        io_task_runner_(std::move(io_task_runner)) {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  ~Client() override { DCHECK(thread_checker_.CalledOnValidThread()); }

  // JpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t bitstream_buffer_id) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (owner_)
      owner_->NotifyDecodeStatus(route_id_, bitstream_buffer_id,
                                 JpegDecodeAccelerator::NO_ERRORS);
  }

  void NotifyError(int32_t bitstream_buffer_id,
                   JpegDecodeAccelerator::Error error) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (owner_)
      owner_->NotifyDecodeStatus(route_id_, bitstream_buffer_id, error);
  }

  void Decode(const BitstreamBuffer& bitstream_buffer,
              const scoped_refptr<VideoFrame>& video_frame) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    DCHECK(accelerator_);
    accelerator_->Decode(bitstream_buffer, video_frame);
  }

  void set_accelerator(std::unique_ptr<JpegDecodeAccelerator> accelerator) {
    DCHECK(thread_checker_.CalledOnValidThread());
    accelerator_ = std::move(accelerator);
  }

 private:
  base::ThreadChecker thread_checker_;
  base::WeakPtr<GpuJpegDecodeAccelerator> owner_;
  const int32_t route_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<JpegDecodeAccelerator> accelerator_;
};

// Create, destroy, and RemoveClient run on child thread. All other methods run
// on IO thread.
// TODO(chfremer): Migrate this to Mojo. See https://crbug.com/699255
class GpuJpegDecodeAccelerator::MessageFilter : public IPC::MessageFilter {
 public:
  explicit MessageFilter(GpuJpegDecodeAccelerator* owner)
      : owner_(owner->AsWeakPtr()),
        child_task_runner_(owner_->child_task_runner_),
        io_task_runner_(owner_->io_task_runner_) {}

  void OnChannelError() override { sender_ = nullptr; }

  void OnChannelClosing() override { sender_ = nullptr; }

  void OnFilterAdded(IPC::Channel* channel) override { sender_ = channel; }

  bool OnMessageReceived(const IPC::Message& msg) override {
    const int32_t route_id = msg.routing_id();
    if (client_map_.find(route_id) == client_map_.end())
      return false;

    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MessageFilter, msg, &route_id)
      IPC_MESSAGE_HANDLER(AcceleratedJpegDecoderMsg_Decode, OnDecodeOnIOThread)
      IPC_MESSAGE_HANDLER(AcceleratedJpegDecoderMsg_Destroy,
                          OnDestroyOnIOThread)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  bool SendOnIOThread(IPC::Message* message) {
    DCHECK(!message->is_sync());
    if (!sender_) {
      delete message;
      return false;
    }
    return sender_->Send(message);
  }

  void AddClientOnIOThread(int32_t route_id,
                           Client* client,
                           base::Callback<void(bool)> response) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    DCHECK(client_map_.count(route_id) == 0);

    // See the comment on GpuJpegDecodeAccelerator::AddClient.
    client_map_[route_id] = base::WrapUnique(client);
    response.Run(true);
  }

  void OnDestroyOnIOThread(const int32_t* route_id) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    const auto& it = client_map_.find(*route_id);
    DCHECK(it != client_map_.end());
    std::unique_ptr<Client> client = std::move(it->second);
    DCHECK(client);
    client_map_.erase(it);

    child_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&MessageFilter::DestroyClient, this, base::Passed(&client)));
  }

  void DestroyClient(std::unique_ptr<Client> client) {
    DCHECK(child_task_runner_->BelongsToCurrentThread());
    if (owner_)
      owner_->ClientRemoved();
    // |client| is destroyed when the scope of this function is left.
  }

  void NotifyDecodeStatusOnIOThread(int32_t route_id,
                                    int32_t buffer_id,
                                    JpegDecodeAccelerator::Error error) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    SendOnIOThread(new AcceleratedJpegDecoderHostMsg_DecodeAck(
        route_id, buffer_id, error));
  }

  void OnDecodeOnIOThread(
      const int32_t* route_id,
      const AcceleratedJpegDecoderMsg_Decode_Params& params) {
    DCHECK(io_task_runner_->BelongsToCurrentThread());
    DCHECK(route_id);
    TRACE_EVENT0("jpeg", "GpuJpegDecodeAccelerator::MessageFilter::OnDecode");

    if (!VerifyDecodeParams(params)) {
      NotifyDecodeStatusOnIOThread(*route_id, params.input_buffer.id(),
                                   JpegDecodeAccelerator::INVALID_ARGUMENT);
      if (base::SharedMemory::IsHandleValid(params.output_video_frame_handle))
        base::SharedMemory::CloseHandle(params.output_video_frame_handle);
      return;
    }

    // For handles in |params|, from now on, |params.output_video_frame_handle|
    // is taken cared by scoper. |params.input_buffer.handle()| need to be
    // closed manually for early exits.
    std::unique_ptr<base::SharedMemory> output_shm(
        new base::SharedMemory(params.output_video_frame_handle, false));
    if (!output_shm->Map(params.output_buffer_size)) {
      LOG(ERROR) << "Could not map output shared memory for input buffer id "
                 << params.input_buffer.id();
      NotifyDecodeStatusOnIOThread(*route_id, params.input_buffer.id(),
                                   JpegDecodeAccelerator::PLATFORM_FAILURE);
      base::SharedMemory::CloseHandle(params.input_buffer.handle());
      return;
    }

    uint8_t* shm_memory = static_cast<uint8_t*>(output_shm->memory());
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalSharedMemory(
        PIXEL_FORMAT_I420,                 // format
        params.coded_size,                 // coded_size
        gfx::Rect(params.coded_size),      // visible_rect
        params.coded_size,                 // natural_size
        shm_memory,                        // data
        params.output_buffer_size,         // data_size
        params.output_video_frame_handle,  // handle
        0,                                 // data_offset
        base::TimeDelta());                // timestamp
    if (!frame.get()) {
      LOG(ERROR) << "Could not create VideoFrame for input buffer id "
                 << params.input_buffer.id();
      NotifyDecodeStatusOnIOThread(*route_id, params.input_buffer.id(),
                                   JpegDecodeAccelerator::PLATFORM_FAILURE);
      base::SharedMemory::CloseHandle(params.input_buffer.handle());
      return;
    }
    frame->AddDestructionObserver(
        base::Bind(DecodeFinished, base::Passed(&output_shm)));

    DCHECK_GT(client_map_.count(*route_id), 0u);
    Client* client = client_map_[*route_id].get();
    client->Decode(params.input_buffer, frame);
  }

 protected:
  ~MessageFilter() override {
    if (client_map_.empty())
      return;

    if (child_task_runner_->BelongsToCurrentThread()) {
      client_map_.clear();
    } else {
      // Make sure |Client| are deleted on child thread.
      std::unique_ptr<ClientMap> client_map(new ClientMap);
      client_map->swap(client_map_);

      child_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&DeleteClientMapOnChildThread, base::Passed(&client_map)));
    }
  }

 private:
  using ClientMap = base::hash_map<int32_t, std::unique_ptr<Client>>;

  // Must be static because this method runs after destructor.
  static void DeleteClientMapOnChildThread(
      std::unique_ptr<ClientMap> client_map) {
    // |client_map| is cleared when the scope of this function is left.
  }

  base::WeakPtr<GpuJpegDecodeAccelerator> owner_;

  // GPU child task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The sender to which this filter was added.
  IPC::Sender* sender_;

  // A map from route id to JpegDecodeAccelerator.
  // Unless in destructor (maybe on child thread), |client_map_| should
  // only be accessed on IO thread.
  ClientMap client_map_;
};

GpuJpegDecodeAccelerator::GpuJpegDecodeAccelerator(
    gpu::FilteredSender* channel,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : GpuJpegDecodeAccelerator(
          channel,
          std::move(io_task_runner),
          GpuJpegDecodeAcceleratorFactoryProvider::GetAcceleratorFactories()) {}

GpuJpegDecodeAccelerator::GpuJpegDecodeAccelerator(
    gpu::FilteredSender* channel,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    std::vector<GpuJpegDecodeAcceleratorFactoryProvider::CreateAcceleratorCB>
        accelerator_factory_functions)
    : accelerator_factory_functions_(accelerator_factory_functions),
      channel_(channel),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(io_task_runner)),
      client_number_(0) {}

GpuJpegDecodeAccelerator::~GpuJpegDecodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (filter_) {
    channel_->RemoveFilter(filter_.get());
  }
}

void GpuJpegDecodeAccelerator::AddClient(int32_t route_id,
                                         base::Callback<void(bool)> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegDecoder::Initialize
  // needs to be updated.

  std::unique_ptr<Client> client(
      new Client(AsWeakPtr(), route_id, io_task_runner_));
  std::unique_ptr<JpegDecodeAccelerator> accelerator;
  for (const auto& create_jda_function : accelerator_factory_functions_) {
    std::unique_ptr<JpegDecodeAccelerator> tmp_accelerator =
        create_jda_function.Run(io_task_runner_);
    if (tmp_accelerator && tmp_accelerator->Initialize(client.get())) {
      accelerator = std::move(tmp_accelerator);
      break;
    }
  }

  if (!accelerator) {
    DLOG(ERROR) << "JPEG accelerator Initialize failed";
    response.Run(false);
    return;
  }
  client->set_accelerator(std::move(accelerator));

  if (!filter_) {
    DCHECK_EQ(client_number_, 0);
    filter_ = new MessageFilter(this);
    // This should be before AddClientOnIOThread.
    channel_->AddFilter(filter_.get());
  }
  client_number_++;

  // In this PostTask, |client| may leak if |io_task_runner_| is destroyed
  // before |client| reached AddClientOnIOThread. However we cannot use scoper
  // to protect it because |client| can only be deleted on child thread. The IO
  // thread is destroyed at termination, at which point it's ok to leak since
  // we're going to tear down the process anyway. So we just crossed fingers
  // here instead of making the code unnecessarily complicated.
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MessageFilter::AddClientOnIOThread, filter_,
                            route_id, client.release(), response));
}

void GpuJpegDecodeAccelerator::NotifyDecodeStatus(
    int32_t route_id,
    int32_t buffer_id,
    JpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Send(new AcceleratedJpegDecoderHostMsg_DecodeAck(route_id, buffer_id, error));
}

void GpuJpegDecodeAccelerator::ClientRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(client_number_, 0);
  client_number_--;
  if (client_number_ == 0) {
    channel_->RemoveFilter(filter_.get());
    filter_ = nullptr;
  }
}

bool GpuJpegDecodeAccelerator::Send(IPC::Message* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_->Send(message);
}

void GpuJpegDecodeAccelerator::Initialize(InitializeCallback callback) {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

void GpuJpegDecodeAccelerator::Decode(
    const BitstreamBuffer& input_buffer,
    const gfx::Size& coded_size,
    mojo::ScopedSharedBufferHandle output_handle,
    uint32_t output_buffer_size,
    DecodeCallback callback) {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

void GpuJpegDecodeAccelerator::Uninitialize() {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

}  // namespace media
