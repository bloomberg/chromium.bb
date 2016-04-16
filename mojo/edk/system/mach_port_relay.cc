// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/mach_port_relay.h"

#include <mach/mach.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/mach_port_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/process/process.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

// static
bool MachPortRelay::ReceivePorts(PlatformHandleVector* handles) {
  DCHECK(handles);

  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = handles->data() + i;
    DCHECK(handle->type != PlatformHandle::Type::MACH);
    if (handle->type != PlatformHandle::Type::MACH_NAME)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH;
      continue;
    }

    base::mac::ScopedMachReceiveRight message_port(handle->port);
    base::mac::ScopedMachSendRight received_port(
        base::ReceiveMachPort(message_port.get()));
    if (received_port.get() == MACH_PORT_NULL) {
      handle->port = MACH_PORT_NULL;
      LOG(ERROR) << "Error receiving mach port";
      return false;
    }

    handle->port = received_port.release();
    handle->type = PlatformHandle::Type::MACH;
  }

  return true;
}

MachPortRelay::MachPortRelay(base::PortProvider* port_provider)
    : port_provider_(port_provider) {
  DCHECK(port_provider);
  port_provider_->AddObserver(this);
}

MachPortRelay::~MachPortRelay() {
  port_provider_->RemoveObserver(this);
}

bool MachPortRelay::SendPortsToProcess(Channel::Message* message,
                                       base::ProcessHandle process) {
  DCHECK(message);
  mach_port_t task_port = port_provider_->TaskForPid(process);
  if (task_port == MACH_PORT_NULL)
    return false;

  size_t num_sent = 0;
  bool error = false;
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  // Message should have handles, otherwise there's no point in calling this
  // function.
  DCHECK(handles);
  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = &(*handles)[i];
    DCHECK(handle->type != PlatformHandle::Type::MACH_NAME);
    if (handle->type != PlatformHandle::Type::MACH)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH_NAME;
      num_sent++;
      continue;
    }

    mach_port_name_t intermediate_port;
    intermediate_port = base::CreateIntermediateMachPort(
        task_port, base::mac::ScopedMachSendRight(handle->port), nullptr);
    if (intermediate_port == MACH_PORT_NULL) {
      handle->port = MACH_PORT_NULL;
      error = true;
      break;
    }
    handle->port = intermediate_port;
    handle->type = PlatformHandle::Type::MACH_NAME;
    num_sent++;
  }
  DCHECK(error || num_sent);
  message->SetHandles(std::move(handles));

  return !error;
}

bool MachPortRelay::ExtractPortRights(Channel::Message* message,
                                      base::ProcessHandle process) {
  DCHECK(message);

  mach_port_t task_port = port_provider_->TaskForPid(process);
  if (task_port == MACH_PORT_NULL)
    return false;

  size_t num_received = 0;
  bool error = false;
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  // Message should have handles, otherwise there's no point in calling this
  // function.
  DCHECK(handles);
  for (size_t i = 0; i < handles->size(); i++) {
    PlatformHandle* handle = handles->data() + i;
    DCHECK(handle->type != PlatformHandle::Type::MACH);
    if (handle->type != PlatformHandle::Type::MACH_NAME)
      continue;

    if (handle->port == MACH_PORT_NULL) {
      handle->type = PlatformHandle::Type::MACH;
      num_received++;
      continue;
    }

    mach_port_t extracted_right = MACH_PORT_NULL;
    mach_msg_type_name_t extracted_right_type;
    kern_return_t kr =
        mach_port_extract_right(task_port, handle->port,
                                MACH_MSG_TYPE_MOVE_SEND,
                                &extracted_right, &extracted_right_type);
    if (kr != KERN_SUCCESS) {
      error = true;
      break;
    }

    DCHECK_EQ(static_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
              extracted_right_type);
    handle->port = extracted_right;
    handle->type = PlatformHandle::Type::MACH;
    num_received++;
  }
  DCHECK(error || num_received);
  message->SetHandles(std::move(handles));

  return !error;
}

void MachPortRelay::AddObserver(Observer* observer) {
  base::AutoLock locker(observers_lock_);
  bool inserted = observers_.insert(observer).second;
  DCHECK(inserted);
}

void MachPortRelay::RemoveObserver(Observer* observer) {
  base::AutoLock locker(observers_lock_);
  observers_.erase(observer);
}

void MachPortRelay::OnReceivedTaskPort(base::ProcessHandle process) {
  base::AutoLock locker(observers_lock_);
  for (const auto observer : observers_)
    observer->OnProcessReady(process);
}

}  // namespace edk
}  // namespace mojo
