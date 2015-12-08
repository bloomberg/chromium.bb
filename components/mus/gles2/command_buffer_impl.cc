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
      driver_(driver.Pass()),
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
    mus::mojom::CommandBufferSyncClientPtr sync_client,
    mus::mojom::CommandBufferSyncPointClientPtr sync_point_client,
    mus::mojom::CommandBufferLostContextObserverPtr loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  sync_point_client_ = sync_point_client.Pass();
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::InitializeHelper,
                 base::Unretained(this),
                 base::Passed(&sync_client),
                 base::Passed(&loss_observer),
                 base::Passed(&shared_state), base::Passed(&attribs)));
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

void CommandBufferImpl::MakeProgress(int32_t last_get_offset) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::MakeProgressHelper,
                 base::Unretained(this), last_get_offset));
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

void CommandBufferImpl::InsertSyncPoint(bool retire) {
  uint32_t sync_point = gpu_state_->sync_point_manager()->GenerateSyncPoint();
  sync_point_client_->DidInsertSyncPoint(sync_point);
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

void CommandBufferImpl::Echo(const mojo::Callback<void()>& callback) {
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::EchoHelper, base::Unretained(this),
          FROM_HERE,
          gpu_state_->control_task_runner(),
          base::Bind(&RunCallback, callback)));
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
      new mojo::Binding<mus::mojom::CommandBuffer>(this, request.Pass()));
  binding_->set_connection_error_handler([this]() { OnConnectionError(); });
}

bool CommandBufferImpl::InitializeHelper(
    mojom::CommandBufferSyncClientPtr sync_client,
    mojom::CommandBufferLostContextObserverPtr loss_observer,
    mojo::ScopedSharedBufferHandle shared_state,
    mojo::Array<int32_t> attribs) {
  DCHECK(driver_->IsScheduled());
  driver_->Initialize(sync_client.PassInterface(),
                      loss_observer.PassInterface(),
                      shared_state.Pass(),
                      attribs.Pass());
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

bool CommandBufferImpl::MakeProgressHelper(int32_t last_get_offset) {
  DCHECK(driver_->IsScheduled());
  driver_->MakeProgress(last_get_offset);
  return true;
}

bool CommandBufferImpl::RegisterTransferBufferHelper(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(driver_->IsScheduled());
  driver_->RegisterTransferBuffer(id, transfer_buffer.Pass(), size);
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

bool CommandBufferImpl::EchoHelper(
    const tracked_objects::Location& from_here,
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner,
    const base::Closure& reply) {
  origin_task_runner->PostTask(from_here, reply);
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
  driver_->CreateImage(id, memory_handle.Pass(), type, size.Pass(), format,
      internal_format);
  return true;
}

bool CommandBufferImpl::DestroyImageHelper(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyImage(id);
  return true;
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
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferImpl::DeleteHelper, base::Unretained(this)));
}

bool CommandBufferImpl::DeleteHelper() {
  delete this;
  return true;
}

}  // namespace mus
