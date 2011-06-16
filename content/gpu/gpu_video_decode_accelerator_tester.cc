// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(vmr): Once done with the implementation, add conclusive documentation
//            what you can do with this tester, how you can configure it and how
//            you can extend it.

#include <fstream>
#include <ios>
#include <iostream>
#include <new>
#include <vector>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/common/common_param_traits.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "media/base/bitstream_buffer.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::InvokeArgument;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

// Route ID.
static const int32 kRouteId = 99;

class MockGpuChannel : public IPC::Message::Sender {
 public:
  MockGpuChannel() {}
  virtual ~MockGpuChannel() {}

  // IPC::Message::Sender implementation.
  MOCK_METHOD1(Send, bool(IPC::Message* msg));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuChannel);
};

class MockVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
 public:
  MockVideoDecodeAccelerator() {}
  virtual ~MockVideoDecodeAccelerator() {}

  MOCK_METHOD1(GetConfig,
               const std::vector<uint32>&
                   (const std::vector<uint32>& prototype_config));
  MOCK_METHOD1(Initialize, bool(const std::vector<uint32>& config));
  MOCK_METHOD2(Decode, bool(media::BitstreamBuffer* bitstream_buffer,
                            media::VideoDecodeAcceleratorCallback* callback));
  MOCK_METHOD1(AssignPictureBuffer,
               void(std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
                        picture_buffers));
  MOCK_METHOD1(ReusePictureBuffer,
               void(media::VideoDecodeAccelerator::PictureBuffer*
                    picture_buffer));
  MOCK_METHOD1(Flush, bool(media::VideoDecodeAcceleratorCallback* callback));
  MOCK_METHOD1(Abort, bool(media::VideoDecodeAcceleratorCallback* callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecodeAccelerator);
};

// Pull-based video source to read video data from a file.
// TODO(vmr): Make this less of a memory hog, Now reads whole file into mem in
//            the beginning. Do this when it actually becomes a problem.
class TestVideoSource {
 public:
  TestVideoSource()
      : file_length_(0),
        offset_(0) {}

  ~TestVideoSource() {}

  bool Open(const std::string& url) {
    // TODO(vmr): Use file_util::ReadFileToString or equivalent to read the file
    //            if one-shot reading is used.
    scoped_ptr<std::ifstream> file;
    file.reset(
        new std::ifstream(url.c_str(),
                          std::ios::in | std::ios::binary | std::ios::ate));
    if (!file->good()) {
      DLOG(ERROR) << "Failed to open file \"" << url << "\".";
      return false;
    }
    file->seekg(0, std::ios::end);
    uint32 length = file->tellg();
    file->seekg(0, std::ios::beg);
    mem_.reset(new uint8[length]);
    DCHECK(mem_.get());  // Simply assumed to work on tester.
    file->read(reinterpret_cast<char*>(mem_.get()), length);
    file_length_ = length;
    file->close();
    return true;
  }

  // Reads next packet from the input stream.
  // Returns number of read bytes on success, 0 on when there was no valid data
  // to be read and -1 if user gave NULL or too small buffer.
  // TODO(vmr): Modify to differentiate between errors and EOF.
  int32 Read(uint8* target_mem, uint32 size) {
    if (!target_mem)
      return -1;
    uint8* unit_begin = NULL;
    uint8* unit_end = NULL;
    uint8* ptr = mem_.get() + offset_;
    while (offset_ + 4 < file_length_) {
      if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 0 && ptr[3] == 1) {
        // start code found
        if (!unit_begin) {
          unit_begin = ptr;
        } else {
          // back-up 1 byte.
          unit_end = ptr;
          break;
        }
      }
      ptr++;
      offset_++;
    }
    if (unit_begin && offset_ + 4 == file_length_) {
      // Last unit. Set the unit_end to point to the last byte.
      unit_end = ptr + 4;
      offset_ += 4;
    } else if (!unit_begin || !unit_end) {
      // No unit start codes found in buffer.
      return 0;
    }
    if (static_cast<int32>(size) >= unit_end - unit_begin) {
      memcpy(target_mem, unit_begin, unit_end - unit_begin);
      return unit_end - unit_begin;
    }
    // Rewind to the beginning start code if there is one as it should be
    // returned with next Read().
    offset_ = unit_begin - mem_.get();
    return -1;
  }

 private:
  uint32 file_length_;
  uint32 offset_;
  scoped_array<uint8> mem_;
};

// Class for posting QuitTask to other message loop when observed message loop
// is quitting. Observer must be added to the message loop by calling
// AddDestructionObserver on the message loop to be tracked.
class QuitObserver
    : public MessageLoop::DestructionObserver {
 public:
  explicit QuitObserver(MessageLoop* loop_to_quit)
      : loop_to_quit_(loop_to_quit) {
  }
  ~QuitObserver() {}

  void WillDestroyCurrentMessageLoop() {
    loop_to_quit_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 protected:
  MessageLoop* loop_to_quit_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(QuitObserver);
};

// FakeClient is a class that mimics normal operation from the client
// process perspective. Underlying code will be receiving IPC commands from the
// FakeClient and sending IPC commands back to the FakeClient just as the
// underlying code would do with actual process running on top of it.
class FakeClient
    : public base::RefCountedThreadSafe<FakeClient>,
      public IPC::Message::Sender,
      public base::PlatformThread::Delegate {
 public:
  FakeClient(MessageLoop* message_loop_for_quit,
             IPC::Channel::Listener* gpu_video_decode_accelerator_,
             std::string video_source_filename)
      : fake_gpu_process_(gpu_video_decode_accelerator_),
        message_loop_for_quit_(message_loop_for_quit),
        test_video_source_(),
        assigned_buffer_count_(0),
        end_of_stream_(false),
        error_(false),
        thread_initialized_event_(true, false),  // Manual reset & unsignalled.
        video_source_filename_(video_source_filename) {
    // Start-up the thread for processing incoming IPC from GpuVideoDecoder and
    // wait until it has finished setting up the message loop.
    base::PlatformThread::Create(0, this, &thread_handle_);
    while (!thread_initialized_event_.Wait()) {
      // Do nothing but wait.
    };

    // Create 1Mb of shared memory.
    shm_.reset(new base::SharedMemory());
    CHECK(shm_->CreateAndMapAnonymous(1024 * 1024));
  }

  virtual ~FakeClient() {
    DCHECK_EQ(assigned_buffer_count_, 0);
  }

  bool DispatchFirstDecode() {
    if (!test_video_source_.Open(video_source_filename_)) {
      return false;
    }
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &FakeClient::OnBitstreamBufferProcessed,
                          base::SharedMemory::NULLHandle()));
    return true;
  }

  // PlatformThread::Delegate implementation, i.e. the thread where the
  // GpuVideoDecoder IPC handling executes.
  virtual void ThreadMain() {
    message_loop_.reset(new MessageLoop());
    // Set the test done observer to observe when client is done.
    test_done_observer_.reset(new QuitObserver(message_loop_for_quit_));
    message_loop_->AddDestructionObserver(test_done_observer_.get());
    thread_initialized_event_.Signal();
    message_loop_->Run();
    message_loop_.reset(); // Destroy the message_loop_.
  }

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg) {
    // Dispatch the message loops to the single message loop which we want to
    // execute all the simulated IPC.
    if (MessageLoop::current() != message_loop_) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(this,
                            &FakeClient::Send,
                            msg));
      return true;
    }
    // This execution should happen in the context of our fake GPU thread.
    CHECK(msg);
    LogMessage(msg);
    IPC_BEGIN_MESSAGE_MAP(FakeClient, *msg)
      IPC_MESSAGE_HANDLER(
          AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed,
          OnBitstreamBufferProcessed)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers,
                          OnProvidePictureBuffers)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_DismissPictureBuffer,
                          OnDismissPictureBuffer)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_PictureReady,
                          OnPictureReady)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_FlushDone,
                          OnFlushDone)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_AbortDone,
                          OnAbortDone)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_EndOfStream,
                          OnEndOfStream)
      IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderHostMsg_ErrorNotification,
                          OnError)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()
    delete msg;
    return true;
  }

  virtual void OnBitstreamBufferProcessed(base::SharedMemoryHandle handle) {
    // No action on end of stream.
    if (end_of_stream_)
      return;
    uint32 read = test_video_source_.Read(
                      reinterpret_cast<uint8*>(shm_->memory()),
                      shm_->created_size());
    if (read > 0) {
      AcceleratedVideoDecoderMsg_Decode msg(kRouteId,
                                            shm_->handle(),
                                            0,
                                            read);
      fake_gpu_process_->OnMessageReceived(msg);
    } else if (read == 0) {
      // If no more data, flush to get the rest out.
      end_of_stream_ = true;
      AcceleratedVideoDecoderMsg_Flush msg(kRouteId);
      fake_gpu_process_->OnMessageReceived(msg);
    } else {
      // Error. Let's flush and abort.
      error_ = true;
      AcceleratedVideoDecoderMsg_Flush msg(kRouteId);
      fake_gpu_process_->OnMessageReceived(msg);
    }
  }

  virtual void OnProvidePictureBuffers(int32 num_frames,
                                       std::vector<uint32> config) {
    // Allocate and assign the picture buffers.
    std::vector<uint32> textures;
    // TODO(vmr): Get from config.
    uint32 width = 320;
    uint32 height = 240;
    uint32 bits_per_pixel = 32;
    for (int32 i = 0; i < num_frames; i++) {
      // Create the shared memory, send it and store it into our local map.
      base::SharedMemory* shm = new base::SharedMemory();
      CHECK(shm->CreateAnonymous(width * height * bits_per_pixel / 8));
      shm_map_[next_picture_buffer_id_] = shm;
      AcceleratedVideoDecoderMsg_AssignPictureBuffer msg(
          kRouteId,
          next_picture_buffer_id_,
          shm->handle(),
          textures);
      fake_gpu_process_->OnMessageReceived(msg);
      next_picture_buffer_id_++;
      assigned_buffer_count_++;
    }
  }

  virtual void OnDismissPictureBuffer(int32 picture_buffer_id) {
    // Free previously allocated buffers.
    base::SharedMemory* shm = shm_map_[picture_buffer_id];
    shm_map_.erase(picture_buffer_id);
    delete shm;  // Will also close shared memory.
    assigned_buffer_count_--;
  }

  virtual void OnPictureReady(int32 picture_buffer_id) {
    // Process & recycle picture buffer.
    AcceleratedVideoDecoderMsg_ReusePictureBuffer msg(kRouteId,
                                                      picture_buffer_id);
    fake_gpu_process_->OnMessageReceived(msg);
  }

  virtual void OnFlushDone() {
    // TODO(vmr): Check that we had been actually flushing.
    if (end_of_stream_ || error_) {
      // Send the final Abort request.
      AcceleratedVideoDecoderMsg_Abort msg(kRouteId);
      fake_gpu_process_->OnMessageReceived(msg);
    }
  }

  virtual void OnAbortDone() {
    // Done aborting... case over.
    message_loop_->QuitNow();
  }

  virtual void OnEndOfStream() {
    end_of_stream_ = true;
    AcceleratedVideoDecoderMsg_Flush msg(kRouteId);
    fake_gpu_process_->OnMessageReceived(msg);
  }

  virtual void OnError(uint32 error_id) {
    // Send the final Abort request.
    AcceleratedVideoDecoderMsg_Abort msg(kRouteId);
    fake_gpu_process_->OnMessageReceived(msg);
  }

 private:
  // TODO(vmr): Remove redundant logging for IPC calls with proper Chromium
  //            logging facilities.
  void LogMessage(IPC::Message* msg) {
    switch (msg->type()) {
      case AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed::ID:
        DLOG(INFO) << "CLIENT << "
            "AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed";
        break;
      case AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers::ID:
        DLOG(INFO) << "CLIENT << "
            "AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers";
        break;
      case AcceleratedVideoDecoderHostMsg_DismissPictureBuffer::ID:
        DLOG(INFO) << "CLIENT << "
            "AcceleratedVideoDecoderHostMsg_DismissPictureBuffer";
        break;
      case AcceleratedVideoDecoderHostMsg_PictureReady::ID:
        DLOG(INFO) << "CLIENT << AcceleratedVideoDecoderHostMsg_PictureReady";
        break;
      case AcceleratedVideoDecoderHostMsg_FlushDone::ID:
        DLOG(INFO) << "CLIENT << AcceleratedVideoDecoderHostMsg_FlushDone";
        break;
      case AcceleratedVideoDecoderHostMsg_AbortDone::ID:
        DLOG(INFO) << "CLIENT << AcceleratedVideoDecoderHostMsg_AbortDone";
        break;
      case AcceleratedVideoDecoderHostMsg_EndOfStream::ID:
        DLOG(INFO) << "CLIENT << AcceleratedVideoDecoderHostMsg_EndOfStream";
        break;
      case AcceleratedVideoDecoderHostMsg_ErrorNotification::ID:
        DLOG(INFO) << "CLIENT << "
            "AcceleratedVideoDecoderHostMsg_ErrorNotification";
        break;
       default:
        DLOG(INFO) << "CLIENT << UNKNOWN MESSAGE";
        break;
    }
  }

  // Listener which should receive the messages we decide to generate.
  IPC::Channel::Listener* fake_gpu_process_;
  // Message loop into which we want to post any send messages.
  scoped_ptr<MessageLoop> message_loop_;
  // Message loop into which we want to post quit when we're done.
  MessageLoop* message_loop_for_quit_;
  // Test video source to read our data from.
  TestVideoSource test_video_source_;
  // SharedMemory used for input buffers.
  scoped_ptr<base::SharedMemory> shm_;
  // Incremental picture buffer id.
  uint32 next_picture_buffer_id_;
  // Counter to count assigned buffers.
  int32 assigned_buffer_count_;
  // Counter to count locked buffers.
  uint32 locked_buffer_count_;
  // Flag to determine whether we have received end of stream from decoder.
  bool end_of_stream_;
  // Flag to determine whether we have faced an error.
  bool error_;
  // Event to determine the initialization state of the fake client thread.
  base::WaitableEvent thread_initialized_event_;
  // We own the observer used to track messageloop destruction.
  scoped_ptr<QuitObserver> test_done_observer_;
  // Handle to the thread.
  base::PlatformThreadHandle thread_handle_;
  // File name for video.
  std::string video_source_filename_;
  // Map to hold the picture buffer objects we create.
  std::map<int32, base::SharedMemory*> shm_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FakeClient);
};

// Class to fake the regular behaviour of video decode accelerator.
class FakeVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
 public:
  explicit FakeVideoDecodeAccelerator(
      media::VideoDecodeAccelerator::Client* client)
      : state_(IDLE),
        end_of_stream_(false),
        client_(client),
        output_counter_(0) {
  }
  virtual ~FakeVideoDecodeAccelerator() {}

  virtual const std::vector<uint32>& GetConfig(
      const std::vector<uint32>& prototype_config) {
    return config_;
  }

  virtual bool Initialize(const std::vector<uint32>& config) {
    return config_ == config;
  }

  // |bitstream_buffer| is owned by client and client guarantees it will not
  // release it before |callback| is called.
  virtual bool Decode(media::BitstreamBuffer* bitstream_buffer,
                      media::VideoDecodeAcceleratorCallback* callback) {
    if (assigned_picture_buffers_.empty()) {
      std::vector<uint32> cfg;
      bitstream_buffer_cb_.reset(callback);
      client_->ProvidePictureBuffers(2, cfg);
      return true;
    }
    // Simulate the bitstream processed callback.
    if (!end_of_stream_) {
      callback->Run();
      delete callback;
    }
    // Then call the picture ready.
    // TODO(vmr): Add container for locked picture buffers.
    if (output_counter_ > 300) {
      if (!end_of_stream_) {
        client_->NotifyEndOfStream();
      }
      end_of_stream_ = true;
      return true;
    }
    media::PictureBuffer* picture_buffer =
        reinterpret_cast<media::PictureBuffer*>(assigned_picture_buffers_.at(
            output_counter_ % assigned_picture_buffers_.size()));
    // TODO(vmr): Get the real values.
    gfx::Size size(320, 240);
    std::vector<uint32> color_format;
    // TODO(vmr): Add the correct mechanism to recycle buffers from assigned to
    //            locked and back.
    client_->PictureReady(new media::Picture(picture_buffer, size, size, NULL));
    output_counter_++;
    return true;
  }

  virtual void AssignPictureBuffer(
      std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
          picture_buffers) {
    assigned_picture_buffers_.insert(
        assigned_picture_buffers_.begin(),
        picture_buffers.begin(),
        picture_buffers.end());

    if (EnoughPictureBuffers()) {
      ChangeState(OPERATIONAL);
      bitstream_buffer_cb_->Run();
      bitstream_buffer_cb_.reset();
    }
  }

  virtual void ReusePictureBuffer(
      media::VideoDecodeAccelerator::PictureBuffer* picture_buffer) {
    // TODO(vmr): Move the picture buffer from locked picture buffer container
    // to the assigned picture buffer container.
  }

  bool Flush(media::VideoDecodeAcceleratorCallback* callback) {
    scoped_ptr<media::VideoDecodeAcceleratorCallback> flush_cb(callback);
    if (state_ != OPERATIONAL) {
      // Flush request is accepted only in OPERATIONAL state.
      return false;
    }
    ChangeState(FLUSHING);
    ChangeState(OPERATIONAL);
    flush_cb->Run();
    return true;
  }

  bool Abort(media::VideoDecodeAcceleratorCallback* callback) {
    scoped_ptr<media::VideoDecodeAcceleratorCallback> abort_cb(callback);
    if (state_ == UNINITIALIZED || state_ == ABORTING) {
      // Flush requested accepted in all other states.
      return false;
    }
    ChangeState(ABORTING);
    // Stop the component here.
    // As buffers are released callback for each buffer DismissBuffer.
    while (!assigned_picture_buffers_.empty()) {
      client_->DismissPictureBuffer(assigned_picture_buffers_.back());
      assigned_picture_buffers_.pop_back();
    }
    ChangeState(IDLE);
    abort_cb->Run();
    return true;
  }

 private:
  enum DecodingState {
    UNINITIALIZED,  // Component has not been configured.
    IDLE,  // Component has been initialized but does not have needed resources.
    OPERATIONAL,  // Component is operational with all resources assigned.
    FLUSHING,  // Component is flushing.
    ABORTING,  // Component is aborting.
  } state_;

  static const char* kStateString[5];

  void ChangeState(DecodingState new_state) {
    DLOG(INFO) << "VideoDecodeAccelerator state change: "
        << kStateString[state_] << " => " << kStateString[new_state];
    state_ = new_state;
  }

  bool EnoughPictureBuffers() {
    return assigned_picture_buffers_.size() >= 2;
  }

  bool end_of_stream_;
  std::vector<uint32> config_;
  std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
      assigned_picture_buffers_;
  std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
      locked_picture_buffers;
  media::VideoDecodeAccelerator::Client* client_;
  scoped_ptr<media::VideoDecodeAcceleratorCallback> bitstream_buffer_cb_;
  uint32 output_counter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FakeVideoDecodeAccelerator);
};

const char* FakeVideoDecodeAccelerator::kStateString[5] = {
  "UNINITIALIZED",
  "IDLE",
  "OPERATIONAL",
  "FLUSHING",
  "ABORTING",
};

// Class to simulate the GpuThread. It will simply simulate the listener
// interface to GPU video decoder by making sure they get ran in our simulated
// GpuThread's context. The thread where loop is created will be used to
// simulate the GpuThread in the actual gpu process environment. User is
// responsible for calling the Run() when it is ready to start processing the
// input messages. Calling Run() will block until QuitTask has been posted to
// the message loop.
class FakeGpuThread
    : public base::RefCountedThreadSafe<FakeGpuThread>,
      public IPC::Channel::Listener {
 public:
  explicit FakeGpuThread(IPC::Channel::Listener* listener)
      : listener_(listener) {
    message_loop_.reset(new MessageLoop());
  }

  // Run will run the message loop.
  void Run() {
    message_loop_->Run();
  }
  // Returns pointer to the message loop this class has initialized.
  MessageLoop* message_loop() {
    return message_loop_.get();
  }

  // IPC::Channel::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) {
    // Dispatch the message loops to the single message loop which we want to
    // execute all the simulated IPC.
    if (MessageLoop::current() != message_loop_.get()) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(
              this,
              &FakeGpuThread::OnMessageReceived,
              message));
      return true;
    }
    LogMessage(message);
    return listener_->OnMessageReceived(message);
  }
  void OnChannelConnected(int32 peer_pid) {
    // Dispatch the message loops to the single message loop which we want to
    // execute all the simulated IPC.
    if (MessageLoop::current() != message_loop_.get()) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(
              this,
              &FakeGpuThread::OnChannelConnected,
              peer_pid));
      return;
    }
    listener_->OnChannelConnected(peer_pid);
  }
  void OnChannelError() {
    // Dispatch the message loops to the single message loop which we want to
    // execute all the simulated IPC.
    if (MessageLoop::current() != message_loop_.get()) {
      message_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(
              this,
              &FakeGpuThread::OnChannelError));
      return;
    }
    listener_->OnChannelError();
  }

 private:
  // TODO(vmr): Use proper Chrome IPC logging instead.
  void LogMessage(const IPC::Message& msg) {
    switch (msg.type()) {
      case AcceleratedVideoDecoderMsg_GetConfigs::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_GetConfigs";
        break;
      case AcceleratedVideoDecoderMsg_Create::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_Create";
        break;
      case AcceleratedVideoDecoderMsg_Decode::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_Decode";
        break;
      case AcceleratedVideoDecoderMsg_AssignPictureBuffer::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_AssignPictureBuffer";
        break;
      case AcceleratedVideoDecoderMsg_ReusePictureBuffer::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_ReusePictureBuffer";
        break;
      case AcceleratedVideoDecoderMsg_Flush::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_Flush";
        break;
      case AcceleratedVideoDecoderMsg_Abort::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_Abort";
        break;
      case AcceleratedVideoDecoderMsg_Destroy::ID:
        DLOG(INFO) << "GPU << AcceleratedVideoDecoderMsg_Destroy";
        break;
       default:
        DLOG(INFO) << "GPU << UNKNOWN MESSAGE";
        break;
    }
  }

  // Pointer to the listener where the messages are to be dispatched.
  IPC::Channel::Listener* listener_;
  // MessageLoop where the GpuVideoDecodeAccelerator execution is supposed to
  // run in.
  scoped_ptr<MessageLoop> message_loop_;

  // To make sure that only scoped_refptr can release us.
  friend class base::RefCountedThreadSafe<FakeGpuThread>;
  virtual ~FakeGpuThread() {}
};

// GpuVideoDecodeAcceleratorTest is the main test class that owns all the other
// objects and does global setup and teardown.
class GpuVideoDecodeAcceleratorTest
    : public ::testing::TestWithParam<const char*>,
      public IPC::Channel::Listener {
 public:
  GpuVideoDecodeAcceleratorTest() {}
  virtual ~GpuVideoDecodeAcceleratorTest() {}

  // Functions for GUnit test fixture setups.
  void SetUp() {
    // Sets up the message loop pretending the fake client.
    SetUpFakeClientAndGpuThread();

    // Initialize the GPU video decode accelerator with the mock underlying
    // accelerator.
    gpu_video_decode_accelerator_ =
        new GpuVideoDecodeAccelerator(fake_client_.get(), kRouteId);
    gpu_video_decode_accelerator_->set_video_decode_accelerator(
        &mock_video_decode_accelerator_);
    // Set-up the underlying video decode accelerator.
    video_decode_accelerator_.reset(
        new FakeVideoDecodeAccelerator(gpu_video_decode_accelerator_.get()));
    std::vector<uint32> config;
    video_decode_accelerator_->Initialize(config);

    // Set up the default mock behaviour.
    SetUpDefaultMockGpuChannelDelegation();
    SetUpMockVideoDecodeAcceleratorDelegation();
  }

  void Teardown() {}

  void SetUpFakeClientAndGpuThread() {
    // Create the message loop for the IO thread (current thread).
    // Also implements passing channel messages automatically to this thread.
    fake_gpu_thread_ = new FakeGpuThread(this);

    // Initialize the fake client to inform our io message loop and use the
    // fresh FakeGpuThread object to send the simulated IPC to the decoder.
    fake_client_ = new FakeClient(
        fake_gpu_thread_->message_loop(), fake_gpu_thread_.get(),
        GetParam());
  }

  void SetUpDefaultMockGpuChannelDelegation() {
    // Set the MockGpuChannel to call by default always the FakeClient when
    // anything is called from the GpuChannel. This will simulate default flow
    // of the video decoding.
    ON_CALL(mock_gpu_channel_, Send(_))
      .WillByDefault(Invoke(fake_client_.get(), &FakeClient::Send));
  }

  void SetUpMockVideoDecodeAcceleratorDelegation() {
    // Set the MockVideoDecodeAccelerator to call by the used decode
    // accelerator.
    // Default builds against fake.
    ON_CALL(mock_video_decode_accelerator_, GetConfig(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::GetConfig));
    ON_CALL(mock_video_decode_accelerator_, Initialize(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::Initialize));
     ON_CALL(mock_video_decode_accelerator_, Decode(_, _))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::Decode));
    ON_CALL(mock_video_decode_accelerator_, AssignPictureBuffer(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::AssignPictureBuffer));
    ON_CALL(mock_video_decode_accelerator_, ReusePictureBuffer(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::ReusePictureBuffer));
    ON_CALL(mock_video_decode_accelerator_, Flush(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::Flush));
    ON_CALL(mock_video_decode_accelerator_, Abort(_))
      .WillByDefault(Invoke(video_decode_accelerator_.get(),
                            &FakeVideoDecodeAccelerator::Abort));
  }

  // IPC::Channel::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) {
    DCHECK(gpu_video_decode_accelerator_.get());
    return gpu_video_decode_accelerator_->OnMessageReceived(message);
  }
  void OnChannelConnected(int32 peer_pid) {
    DCHECK(gpu_video_decode_accelerator_.get());
    gpu_video_decode_accelerator_->OnChannelConnected(peer_pid);
  }
  void OnChannelError() {
    DCHECK(gpu_video_decode_accelerator_.get());
    gpu_video_decode_accelerator_->OnChannelError();
  }

  // Threading related functions.
  void RunMessageLoop() {
    fake_gpu_thread_->Run();
  }

 protected:
  // We need exit manager to please the message loops we're creating.
  base::AtExitManager exit_manager_;
  // Mock and fake delegate for the IPC channel.
  NiceMock<MockGpuChannel> mock_gpu_channel_;
  // Reference counted pointer to the gpu channel.
  scoped_refptr<FakeClient> fake_client_;
  // Reference counted pointer to the fake gpu channel.
  scoped_refptr<FakeGpuThread> fake_gpu_thread_;
  // Handle to the initialized GpuVideoDecodeAccelerator. Tester owns the
  // GpuVideoDecodeAccelerator.
  scoped_refptr<GpuVideoDecodeAccelerator> gpu_video_decode_accelerator_;
  // Mock and default fake delegate for the underlying video decode accelerator.
  NiceMock<MockVideoDecodeAccelerator> mock_video_decode_accelerator_;
  scoped_ptr<FakeVideoDecodeAccelerator> video_decode_accelerator_;
};

static uint32 kTestH264BitstreamConfig[] = {
  // Intentionally breaking formatting rules to make things more readable.
  media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_FOURCC,
      media::VIDEOCODECFOURCC_H264,
  media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_PROFILE,
      media::H264PROFILE_BASELINE,
  media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_LEVEL,
      media::H264LEVEL_30,
  media::VIDEOATTRIBUTEKEY_BITSTREAMFORMAT_H264_PAYLOADFORMAT,
      media::H264PAYLOADFORMAT_BYTESTREAM,
  media::VIDEOATTRIBUTEKEY_TERMINATOR
};

static uint32 kTestH264BitstreamConfigCount =
    sizeof(kTestH264BitstreamConfig) / sizeof(kTestH264BitstreamConfig[0]);

static std::vector<uint32> kTestH264BitstreamConfigVector(
    kTestH264BitstreamConfig,
    kTestH264BitstreamConfig+kTestH264BitstreamConfigCount);

static uint32 kTestColorConfig[] = {
  media::VIDEOATTRIBUTEKEY_COLORFORMAT_PLANE_PIXEL_SIZE, 16,
  media::VIDEOATTRIBUTEKEY_COLORFORMAT_RED_SIZE, 5,
  media::VIDEOATTRIBUTEKEY_COLORFORMAT_GREEN_SIZE, 6,
  media::VIDEOATTRIBUTEKEY_COLORFORMAT_BLUE_SIZE, 5,
  media::VIDEOATTRIBUTEKEY_TERMINATOR
};

static uint32 kTestColorConfigCount =
    sizeof(kTestColorConfig) / sizeof(kTestColorConfig[0]);

static std::vector<uint32> kTestColorConfigVector(
    kTestColorConfig, kTestColorConfig+kTestColorConfigCount);

MATCHER(ErrorMatcher, std::string("Decoder reported unexpected error")) {
  return arg->type() == AcceleratedVideoDecoderHostMsg_ErrorNotification::ID;
}

TEST_P(GpuVideoDecodeAcceleratorTest, RegularDecodingFlow) {
  // We will carry out the creation of video decoder manually with specified
  // configuration which we know will work. The rest of the functionality is
  // asynchronous and is designed to be tested with this test bench.

  // TODO(vmr): Remove the first decode from here after proper init.
  if (!fake_client_->DispatchFirstDecode()) {
    FAIL() << "Failed to open input file";
    return;
  }

  // TODO(vmr): Verify how we are going to not allow any sending of data.
  // EXPECT_CALL(mock_gpu_channel_, Send(ErrorMatcher()));
  //   .Times(0);

  RunMessageLoop();
  // Once message loop has finished our case is over.
}

const char* kFileNames[] = {
  "media/test/data/test-25fps.h264",
};

INSTANTIATE_TEST_CASE_P(RegularDecodingFlowWithFile,
                        GpuVideoDecodeAcceleratorTest,
                        ::testing::ValuesIn(kFileNames));

int main(int argc, char **argv) {
  // TODO(vmr): Integrate with existing unit test targets.
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
