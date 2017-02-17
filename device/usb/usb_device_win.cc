// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_device_win.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/usb/usb_device_handle.h"

namespace device {

UsbDeviceWin::UsbDeviceWin(
    const std::string& device_path,
    const std::string& hub_path,
    int port_number,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : device_path_(device_path),
      hub_path_(hub_path),
      port_number_(port_number),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      blocking_task_runner_(std::move(blocking_task_runner)) {}

UsbDeviceWin::~UsbDeviceWin() {}

void UsbDeviceWin::Open(const OpenCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
}

}  // namespace device
