// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <string>

#include "base/threading/thread_restrictions.h"
#include "base/tuple.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_service_linux.h"


namespace device {

namespace {

const char kHidrawSubsystem[] = "hidraw";

}  // namespace

HidConnectionLinux::HidConnectionLinux(HidDeviceInfo device_info,
                                       ScopedUdevDevicePtr udev_raw_device)
    : HidConnection(device_info),
      initialized_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());

  udev_device* dev = udev_raw_device.get();
  std::string dev_node;
  if (!FindHidrawDevNode(dev, &dev_node)) {
    LOG(ERROR) << "Cannot open HID device as hidraw device.";
    return;
  }

  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE |
              base::File::FLAG_EXCLUSIVE_READ |
              base::File::FLAG_EXCLUSIVE_WRITE;

  base::File device_file(base::FilePath(dev_node), flags);
  if (!device_file.IsValid()) {
    LOG(ERROR) << device_file.error_details();
    return;
  }
  if (fcntl(device_file.GetPlatformFile(), F_SETFL,
            fcntl(device_file.GetPlatformFile(), F_GETFL) | O_NONBLOCK)) {
    PLOG(ERROR) << "Failed to set non-blocking flag to device file.";
    return;
  }
  device_file_ = device_file.Pass();

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
      device_file_.GetPlatformFile(),
      true,
      base::MessageLoopForIO::WATCH_READ_WRITE,
      &device_file_watcher_,
      this)) {
    LOG(ERROR) << "Cannot start watching file descriptor.";
    return;
  }

  initialized_ = true;
}

HidConnectionLinux::~HidConnectionLinux() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Disconnect();
}

void HidConnectionLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(fd, device_file_.GetPlatformFile());
  DCHECK(initialized_);

  uint8 buffer[1024] = {0};
  int bytes = read(device_file_.GetPlatformFile(), buffer, 1024);
  if (bytes < 0) {
    if (errno == EAGAIN) {
      return;
    }
    Disconnect();
    return;
  }
  scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(bytes));
  memcpy(io_buffer->data(), buffer, bytes);
  input_reports_.push(std::make_pair(io_buffer, bytes));

  ProcessReadQueue();
}

void HidConnectionLinux::OnFileCanWriteWithoutBlocking(int fd) {}

void HidConnectionLinux::Disconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return;

  initialized_ = false;
  device_file_watcher_.StopWatchingFileDescriptor();
  device_file_.Close();
  while (!read_queue_.empty()) {
    PendingRequest callback = read_queue_.front();
    read_queue_.pop();
    callback.c.Run(false, 0);
  }
}

void HidConnectionLinux::Read(scoped_refptr<net::IOBuffer> buffer,
                              size_t size,
                              const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    DCHECK(read_queue_.empty());
    // There might be unread reports.
    if (!input_reports_.empty()){
      read_queue_.push(MakeTuple(buffer, size, callback));
      ProcessReadQueue();
    }
    callback.Run(false, 0);
    return;
  } else {
    read_queue_.push(MakeTuple(buffer, size, callback));
    ProcessReadQueue();
  }
}

void HidConnectionLinux::Write(scoped_refptr<net::IOBuffer> buffer,
                               size_t size,
                               const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    callback.Run(false, 0);
    return;
  } else {
    int bytes = write(device_file_.GetPlatformFile(), buffer->data(), size);
    if (bytes < 0) {
      Disconnect();
      callback.Run(false, 0);
    } else {
      callback.Run(true, bytes);
    }
  }
}

void HidConnectionLinux::GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                          size_t size,
                                          const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    callback.Run(false, 0);
    return;
  }
  NOTIMPLEMENTED();
}

void HidConnectionLinux::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                           size_t size,
                                           const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_) {
    callback.Run(false, 0);
    return;
  }
  NOTIMPLEMENTED();
}

void HidConnectionLinux::ProcessReadQueue() {
  while(read_queue_.size() && input_reports_.size()) {
    PendingRequest request = read_queue_.front();
    read_queue_.pop();
    PendingReport report = input_reports_.front();
    if (report.second > request.b) {
      request.c.Run(false, report.second);
    } else {
      memcpy(request.a->data(), report.first->data(), report.second);
      input_reports_.pop();
      request.c.Run(true, report.second);
    }
  }
}

bool HidConnectionLinux::FindHidrawDevNode(udev_device* parent,
                                           std::string* result) {
  udev* udev = udev_device_get_udev(parent);
  if (!udev)
      return false;

  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  if (!enumerate)
    return false;

  if (udev_enumerate_add_match_subsystem(enumerate.get(), kHidrawSubsystem)) {
    return false;
  }
  if (udev_enumerate_scan_devices(enumerate.get())) {
    return false;
  }

  const char* parent_path = udev_device_get_devpath(parent);
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
      i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr hid_dev(
        udev_device_new_from_syspath(udev, udev_list_entry_get_name(i)));
    const char* raw_path = udev_device_get_devnode(hid_dev.get());
    if (strncmp(parent_path,
                udev_device_get_devpath(hid_dev.get()),
                strlen(parent_path)) == 0 &&
        raw_path) {
      *result = raw_path;
      return true;
    }
  }

  return false;
}

}  // namespace device
