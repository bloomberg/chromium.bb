// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_impl_observer.h"
#include "components/mus/gles2/command_buffer_type_conversions.h"
#include "components/mus/gles2/gpu_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

namespace mus {

namespace {

void RunInitializeCallback(
    const mojom::CommandBuffer::InitializeCallback& callback,
    mojom::CommandBufferInfoPtr info) {
  callback.Run(std::move(info));
}

void RunMakeProgressCallback(
    const mojom::CommandBuffer::MakeProgressCallback& callback,
    mojom::CommandBufferStatePtr state) {
  callback.Run(std::move(state));
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
      driver_(std::move(driver)),
      observer_(nullptr),
      weak_ptr_factory_(this) {
  driver_->set_client(make_scoped_ptr(new CommandBufferDriverClientImpl(this)));

  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferImpl::BindToRequest,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&request)));
}

void CommandBufferImpl::DidLoseContext() {
  OnConnectionError();
}

CommandBufferImpl::~CommandBufferImpl() {
  if (observer_)
    observer_->OnCommandBufferImplDestroyed();
}

void CommandBufferImpl::Initialize(
    mus::mojom::CommandBufferLostContextObserverPtr loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs,
    const mojom::CommandBuffer::InitializeCallback& callback) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::InitializeHelper, base::Unretained(this),
                 base::Passed(&loss_observer), base::Passed(&shared_state),
                 base::Passed(&attribs),
                 base::Bind(&RunInitializeCallback, callback)));
}

void CommandBufferImpl::SetGetBuffer(int32_t buffer) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::SetGetBufferHelper,
                  base::Unretained(this), buffer));
}

void CommandBufferImpl::Flush(int32_t put_offset) {
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  const uint32_t order_num = driver_->sync_point_order_data()
      ->GenerateUnprocessedOrderNumber(sync_point_manager);
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::FlushHelper,
                 base::Unretained(this), put_offset, order_num));
}

void CommandBufferImpl::MakeProgress(
    int32_t last_get_offset,
    const mojom::CommandBuffer::MakeProgressCallback& callback) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferImpl::MakeProgressHelper,
                                base::Unretained(this), last_get_offset,
                                base::Bind(RunMakeProgressCallback, callback)));
}

void CommandBufferImpl::RegisterTransferBuffer(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::RegisterTransferBufferHelper,
                 base::Unretained(this), id,
                 base::Passed(&transfer_buffer), size));
}

void CommandBufferImpl::DestroyTransferBuffer(int32_t id) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::DestroyTransferBufferHelper,
                 base::Unretained(this), id));
}

void CommandBufferImpl::InsertSyncPoint(
    bool retire,
    const mojom::CommandBuffer::InsertSyncPointCallback& callback) {
  uint32_t sync_point = gpu_state_->sync_point_manager()->GenerateSyncPoint();
  callback.Run(sync_point);
  if (retire) {
    gpu_state_->command_buffer_task_runner()->PostTask(
        driver_.get(),
        base::Bind(&CommandBufferImpl::RetireSyncPointHelper,
                   base::Unretained(this),
                   sync_point));
  }
}

void CommandBufferImpl::RetireSyncPoint(uint32_t sync_point) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::RetireSyncPointHelper,
                 base::Unretained(this),
                 sync_point));
}

void CommandBufferImpl::CreateImage(int32_t id,
                                    mojo::ScopedHandle memory_handle,
                                    int32 type,
                                    mojo::SizePtr size,
                                    int32_t format,
                                    int32_t internal_format) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::CreateImageHelper,
                 base::Unretained(this), id,
                 base::Passed(&memory_handle), type,
                 base::Passed(&size), format, internal_format));
}

void CommandBufferImpl::DestroyImage(int32_t id) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::DestroyImageHelper,
                 base::Unretained(this), id));
}

void CommandBufferImpl::BindToRequest(
    mojo::InterfaceRequest<mus::mojom::CommandBuffer> request) {
  binding_.reset(
      new mojo::Binding<mus::mojom::CommandBuffer>(this, std::move(request)));
  binding_->set_connection_error_handler([this]() { OnConnectionError(); });
}

bool CommandBufferImpl::InitializeHelper(
    mojom::CommandBufferLostContextObserverPtr loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs,
    const base::Callback<void(mojom::CommandBufferInfoPtr)>& callback) {
  DCHECK(driver_->IsScheduled());
  bool result =
      driver_->Initialize(loss_observer.PassInterface(),
                          std::move(shared_state), std::move(attribs));
  mojom::CommandBufferInfoPtr info;
  if (result) {
    info = mojom::CommandBufferInfo::New();
    info->command_buffer_namespace = driver_->GetNamespaceID();
    info->command_buffer_id = driver_->GetCommandBufferID();
    info->capabilities =
        mojom::GpuCapabilities::From(driver_->GetCapabilities());
  }
  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&info)));
  return true;
}

bool CommandBufferImpl::SetGetBufferHelper(int32_t buffer) {
  DCHECK(driver_->IsScheduled());
  driver_->SetGetBuffer(buffer);
  return true;
}

bool CommandBufferImpl::FlushHelper(int32_t put_offset,
                                    uint32_t order_num) {
  DCHECK(driver_->IsScheduled());
  driver_->sync_point_order_data()->BeginProcessingOrderNumber(order_num);
  driver_->Flush(put_offset);

  // Return false if the Flush is not finished, so the CommandBufferTaskRunner
  // will not remove this task from the task queue.
  const bool complete = !driver_->HasUnprocessedCommands();
  if (complete)
    driver_->sync_point_order_data()->FinishProcessingOrderNumber(order_num);
  return complete;
}

bool CommandBufferImpl::MakeProgressHelper(
    int32_t last_get_offset,
    const base::Callback<void(mojom::CommandBufferStatePtr)>& callback) {
  DCHECK(driver_->IsScheduled());
  mojom::CommandBufferStatePtr state =
      mojom::CommandBufferState::From(driver_->GetLastState());
  gpu_state_->control_task_runner()->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&state)));
  return true;
}

bool CommandBufferImpl::RegisterTransferBufferHelper(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(driver_->IsScheduled());
  driver_->RegisterTransferBuffer(id, std::move(transfer_buffer), size);
  return true;
}

bool CommandBufferImpl::DestroyTransferBufferHelper(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyTransferBuffer(id);
  return true;
}

bool CommandBufferImpl::RetireSyncPointHelper(uint32_t sync_point) {
  DCHECK(driver_->IsScheduled());
  gpu_state_->sync_point_manager()->RetireSyncPoint(sync_point);
  return true;
}

bool CommandBufferImpl::CreateImageHelper(
    int32_t id,
    mojo::ScopedHandle memory_handle,
    int32_t type,
    mojo::SizePtr size,
    int32_t format,
    int32_t internal_format) {
  DCHECK(driver_->IsScheduled());
  driver_->CreateImage(id, std::move(memory_handle), type, std::move(size),
                       format, internal_format);
  return true;
}

bool CommandBufferImpl::DestroyImageHelper(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyImage(id);
  return true;
}

void CommandBufferImpl::OnConnectionError() {
  // OnConnectionError() is called on the control thread |control_task_runner|.

  // Before deleting, we need to delete |binding_| because it is bound to the
  // current thread (|control_task_runner|).
  binding_.reset();

  // Objects we own (such as CommandBufferDriver) need to be destroyed on the
  // thread we were created on.
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::DeleteHelper, base::Unretained(this)));
}

bool CommandBufferImpl::DeleteHelper() {
  delete this;
  return true;
}

}  // namespace mus
