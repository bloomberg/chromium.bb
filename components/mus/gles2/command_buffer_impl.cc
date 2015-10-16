// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_impl_observer.h"
#include "components/mus/gles2/gpu_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

namespace mus {

namespace {

void RunCallback(const mojo::Callback<void()>& callback) {
  callback.Run();
}

}  // namespace

class CommandBufferImpl::CommandBufferDriverClientImpl
    : public CommandBufferDriver::Client {
 public:
  explicit CommandBufferDriverClientImpl(CommandBufferImpl* command_buffer)
      : command_buffer_(command_buffer) {}

 private:
  void DidLoseContext() override { command_buffer_->DidLoseContext(); }

  CommandBufferImpl* command_buffer_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferDriverClientImpl);
};

CommandBufferImpl::CommandBufferImpl(
    mojo::InterfaceRequest<mus::mojom::CommandBuffer> request,
    scoped_refptr<GpuState> gpu_state,
    scoped_ptr<CommandBufferDriver> driver)
    : gpu_state_(gpu_state),
      driver_task_runner_(base::MessageLoop::current()->task_runner()),
      driver_(driver.Pass()),
      observer_(nullptr),
      weak_ptr_factory_(this) {
  driver_->set_client(make_scoped_ptr(new CommandBufferDriverClientImpl(this)));

  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferImpl::BindToRequest,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&request)));
}

void CommandBufferImpl::Initialize(
    mus::mojom::CommandBufferSyncClientPtr sync_client,
    mus::mojom::CommandBufferSyncPointClientPtr sync_point_client,
    mus::mojom::CommandBufferLostContextObserverPtr loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  sync_point_client_ = sync_point_client.Pass();
  driver_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferDriver::Initialize,
                 base::Unretained(driver_.get()),
                 base::Passed(sync_client.PassInterface()),
                 base::Passed(loss_observer.PassInterface()),
                 base::Passed(&shared_state), base::Passed(&attribs)));
}

void CommandBufferImpl::SetGetBuffer(int32_t buffer) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::SetGetBuffer,
                            base::Unretained(driver_.get()), buffer));
}

void CommandBufferImpl::Flush(int32_t put_offset) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::Flush,
                            base::Unretained(driver_.get()), put_offset));
}

void CommandBufferImpl::MakeProgress(int32_t last_get_offset) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::MakeProgress,
                            base::Unretained(driver_.get()), last_get_offset));
}

void CommandBufferImpl::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::RegisterTransferBuffer,
                            base::Unretained(driver_.get()), id,
                            base::Passed(&transfer_buffer), size));
}

void CommandBufferImpl::DestroyTransferBuffer(int32_t id) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::DestroyTransferBuffer,
                            base::Unretained(driver_.get()), id));
}

void CommandBufferImpl::InsertSyncPoint(bool retire) {
  uint32_t sync_point = gpu_state_->sync_point_manager()->GenerateSyncPoint();
  sync_point_client_->DidInsertSyncPoint(sync_point);
  if (retire) {
    driver_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&gpu::SyncPointManager::RetireSyncPoint,
                   base::Unretained(gpu_state_->sync_point_manager()),
                   sync_point));
  }
}

void CommandBufferImpl::RetireSyncPoint(uint32_t sync_point) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&gpu::SyncPointManager::RetireSyncPoint,
                            base::Unretained(gpu_state_->sync_point_manager()),
                            sync_point));
}

void CommandBufferImpl::Echo(const mojo::Callback<void()>& callback) {
  driver_task_runner_->PostTaskAndReply(FROM_HERE, base::Bind(&base::DoNothing),
                                        base::Bind(&RunCallback, callback));
}

void CommandBufferImpl::CreateImage(int32_t id,
                                    mojo::ScopedHandle memory_handle,
                                    int32 type,
                                    mojo::SizePtr size,
                                    int32_t format,
                                    int32_t internal_format) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::CreateImage,
                            base::Unretained(driver_.get()), id,
                            base::Passed(&memory_handle), type,
                            base::Passed(&size), format, internal_format));
}

void CommandBufferImpl::DestroyImage(int32_t id) {
  driver_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferDriver::DestroyImage,
                            base::Unretained(driver_.get()), id));
}

CommandBufferImpl::~CommandBufferImpl() {
  if (observer_)
    observer_->OnCommandBufferImplDestroyed();
}

void CommandBufferImpl::BindToRequest(
    mojo::InterfaceRequest<mus::mojom::CommandBuffer> request) {
  binding_.reset(
      new mojo::Binding<mus::mojom::CommandBuffer>(this, request.Pass()));
  binding_->set_connection_error_handler([this]() { OnConnectionError(); });
}

void CommandBufferImpl::OnConnectionError() {
  // OnConnectionError() is called on the control thread |control_task_runner|.
  // sync_point_client_ is assigned and accessed on the control thread and so it
  // should also be destroyed on the control because InterfacePtrs are thread-
  // hostile.
  sync_point_client_.reset();

  // Before deleting, we need to delete |binding_| because it is bound to the
  // current thread (|control_task_runner|).
  binding_.reset();

  // Objects we own (such as CommandBufferDriver) need to be destroyed on the
  // thread we were created on.
  driver_task_runner_->DeleteSoon(FROM_HERE, this);
}

void CommandBufferImpl::DidLoseContext() {
  OnConnectionError();
}

}  // namespace mus
