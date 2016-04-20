// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_arc_video_service.h"

#include <fcntl.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/gpu/arc_gpu_video_decode_accelerator.h"
#include "chrome/gpu/arc_video_accelerator.h"
#include "components/arc/common/video_accelerator.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::BufferMetadataPtr,
                     chromeos::arc::BufferMetadata> {
  static arc::mojom::BufferMetadataPtr Convert(
      const chromeos::arc::BufferMetadata& input) {
    arc::mojom::BufferMetadataPtr result = arc::mojom::BufferMetadata::New();
    result->timestamp = input.timestamp;
    result->flags = input.flags;
    result->bytes_used = input.bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<chromeos::arc::BufferMetadata,
                     arc::mojom::BufferMetadataPtr> {
  static chromeos::arc::BufferMetadata Convert(
      const arc::mojom::BufferMetadataPtr& input) {
    chromeos::arc::BufferMetadata result;
    result.timestamp = input->timestamp;
    result.flags = input->flags;
    result.bytes_used = input->bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<arc::mojom::VideoFormatPtr, chromeos::arc::VideoFormat> {
  static arc::mojom::VideoFormatPtr Convert(
      const chromeos::arc::VideoFormat& input) {
    arc::mojom::VideoFormatPtr result = arc::mojom::VideoFormat::New();
    result->pixel_format = input.pixel_format;
    result->buffer_size = input.buffer_size;
    result->min_num_buffers = input.min_num_buffers;
    result->coded_width = input.coded_width;
    result->coded_height = input.coded_height;
    result->crop_left = input.crop_left;
    result->crop_width = input.crop_width;
    result->crop_top = input.crop_top;
    result->crop_height = input.crop_height;
    return result;
  }
};

template <>
struct TypeConverter<chromeos::arc::ArcVideoAccelerator::Config,
                     arc::mojom::ArcVideoAcceleratorConfigPtr> {
  static chromeos::arc::ArcVideoAccelerator::Config Convert(
      const arc::mojom::ArcVideoAcceleratorConfigPtr& input) {
    chromeos::arc::ArcVideoAccelerator::Config result;
    result.device_type =
        static_cast<chromeos::arc::ArcVideoAccelerator::Config::DeviceType>(
            input->device_type);
    result.num_input_buffers = input->num_input_buffers;
    result.input_pixel_format = input->input_pixel_format;
    return result;
  }
};

}  // namespace mojo

namespace chromeos {
namespace arc {

class GpuArcVideoService::AcceleratorStub
    : public ::arc::mojom::VideoAcceleratorService,
      public ArcVideoAccelerator::Client {
 public:
  // |owner| outlives AcceleratorStub.
  explicit AcceleratorStub(GpuArcVideoService* owner)
      : owner_(owner), binding_(this) {}

  ~AcceleratorStub() override { DCHECK(thread_checker_.CalledOnValidThread()); }

  bool Connect(const std::string& token) {
    DVLOG(2) << "Connect";

    mojo::ScopedMessagePipeHandle server_pipe =
        mojo::edk::CreateParentMessagePipe(token);
    if (!server_pipe.is_valid()) {
      LOG(ERROR) << "Invalid pipe";
      return false;
    }

    client_.Bind(
        mojo::InterfacePtrInfo<::arc::mojom::VideoAcceleratorServiceClient>(
            std::move(server_pipe), 0u));

    // base::Unretained is safe because we own |client_|
    client_.set_connection_error_handler(
        base::Bind(&GpuArcVideoService::AcceleratorStub::OnConnectionError,
                   base::Unretained(this)));

    accelerator_.reset(new ArcGpuVideoDecodeAccelerator());

    ::arc::mojom::VideoAcceleratorServicePtr service;
    binding_.Bind(GetProxy(&service));
    // base::Unretained is safe because we own |binding_|
    binding_.set_connection_error_handler(
        base::Bind(&GpuArcVideoService::AcceleratorStub::OnConnectionError,
                   base::Unretained(this)));

    client_->Init(std::move(service));
    return true;
  }

  void OnConnectionError() {
    DVLOG(2) << "OnConnectionError";
    owner_->RemoveClient(this);
    // |this| is deleted.
  }

  // ArcVideoAccelerator::Client implementation:
  void OnError(ArcVideoAccelerator::Error error) override {
    DVLOG(2) << "OnError " << error;
    client_->OnError(
        static_cast<::arc::mojom::VideoAcceleratorServiceClient::Error>(error));
  }

  void OnBufferDone(PortType port,
                    uint32_t index,
                    const BufferMetadata& metadata) override {
    DVLOG(2) << "OnBufferDone " << port << "," << index;
    client_->OnBufferDone(static_cast<::arc::mojom::PortType>(port), index,
                          ::arc::mojom::BufferMetadata::From(metadata));
  }

  void OnResetDone() override {
    DVLOG(2) << "OnResetDone";
    client_->OnResetDone();
  }

  void OnOutputFormatChanged(const VideoFormat& format) override {
    DVLOG(2) << "OnOutputFormatChanged";
    client_->OnOutputFormatChanged(::arc::mojom::VideoFormat::From(format));
  }

  // ::arc::mojom::VideoAcceleratorService impementation.
  void Initialize(::arc::mojom::ArcVideoAcceleratorConfigPtr config,
                  const InitializeCallback& callback) override {
    DVLOG(2) << "Initialize";
    bool result = accelerator_->Initialize(
        config.To<ArcVideoAccelerator::Config>(), this);
    callback.Run(result);
  }

  void BindSharedMemory(::arc::mojom::PortType port,
                        uint32_t index,
                        mojo::ScopedHandle ashmem_handle,
                        uint32_t offset,
                        uint32_t length) override {
    DVLOG(2) << "BindSharedMemoryCallback port=" << port << ", index=" << index
             << ", offset=" << offset << ", length=" << length;

    // TODO(kcwu) make sure do we need special care for invalid handle?
    mojo::edk::ScopedPlatformHandle scoped_platform_handle;
    MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
        ashmem_handle.release().value(), &scoped_platform_handle);
    DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

    auto fd = base::ScopedFD(scoped_platform_handle.release().handle);
    accelerator_->BindSharedMemory(static_cast<PortType>(port), index,
                                   std::move(fd), offset, length);
  }

  void BindDmabuf(::arc::mojom::PortType port,
                  uint32_t index,
                  mojo::ScopedHandle dmabuf_handle) override {
    DVLOG(2) << "BindDmabuf port=" << port << ", index=" << index;
    mojo::edk::ScopedPlatformHandle scoped_platform_handle;
    MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
        dmabuf_handle.release().value(), &scoped_platform_handle);
    DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

    auto fd = base::ScopedFD(scoped_platform_handle.release().handle);
    accelerator_->BindDmabuf(static_cast<PortType>(port), index, std::move(fd));
  }

  void UseBuffer(::arc::mojom::PortType port,
                 uint32_t index,
                 ::arc::mojom::BufferMetadataPtr metadata) override {
    DVLOG(2) << "UseBuffer port=" << port << ", index=" << index;
    accelerator_->UseBuffer(static_cast<PortType>(port), index,
                            metadata.To<BufferMetadata>());
  }

  void SetNumberOfOutputBuffers(uint32_t number) override {
    DVLOG(2) << "SetNumberOfOutputBuffers number=" << number;
    accelerator_->SetNumberOfOutputBuffers(number);
  }

  void Reset() override { accelerator_->Reset(); }

 private:
  base::ThreadChecker thread_checker_;
  GpuArcVideoService* const owner_;
  std::unique_ptr<ArcVideoAccelerator> accelerator_;
  ::arc::mojom::VideoAcceleratorServiceClientPtr client_;
  mojo::Binding<::arc::mojom::VideoAcceleratorService> binding_;
};

GpuArcVideoService::GpuArcVideoService(
    mojo::InterfaceRequest<::arc::mojom::VideoHost> request)
    : binding_(this, std::move(request)) {}

GpuArcVideoService::~GpuArcVideoService() {}

void GpuArcVideoService::OnRequestArcVideoAcceleratorChannel(
    const OnRequestArcVideoAcceleratorChannelCallback& callback) {
  DVLOG(1) << "OnRequestArcVideoAcceleratorChannelCallback";

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle = 0;
  mojo::edk::ScopedPlatformHandle child_handle =
      mojo::edk::ChildProcessLaunched(kUnusedChildProcessHandle);

  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      std::move(child_handle), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }

  std::unique_ptr<AcceleratorStub> stub(new AcceleratorStub(this));

  std::string token = mojo::edk::GenerateRandomToken();
  if (!stub->Connect(token)) {
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  accelerator_stubs_.insert(std::make_pair(stub.get(), std::move(stub)));

  callback.Run(mojo::ScopedHandle(mojo::Handle(wrapped_handle)), token);
}

void GpuArcVideoService::RemoveClient(AcceleratorStub* stub) {
  accelerator_stubs_.erase(stub);
}

}  // namespace arc
}  // namespace chromeos
