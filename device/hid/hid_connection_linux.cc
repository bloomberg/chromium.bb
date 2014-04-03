// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <string>

#include "base/files/file_path.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "base/tuple.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_service_linux.h"

// These are already defined in newer versions of linux/hidraw.h.
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#endif
#ifndef HIDIOCGFEATURE
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

namespace device {

namespace {

// Copies a buffer into a new one with a report ID byte inserted at the front.
scoped_refptr<net::IOBufferWithSize> CopyBufferWithReportId(
    scoped_refptr<net::IOBufferWithSize> buffer,
    uint8_t report_id) {
  scoped_refptr<net::IOBufferWithSize> new_buffer(
      new net::IOBufferWithSize(buffer->size() + 1));
  new_buffer->data()[0] = report_id;
  memcpy(new_buffer->data() + 1, buffer->data(), buffer->size());
  return new_buffer;
}

const char kHidrawSubsystem[] = "hidraw";

}  // namespace

HidConnectionLinux::HidConnectionLinux(HidDeviceInfo device_info,
                                       ScopedUdevDevicePtr udev_raw_device)
    : HidConnection(device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  udev_device* dev = udev_raw_device.get();
  std::string dev_node;
  if (!FindHidrawDevNode(dev, &dev_node)) {
    LOG(ERROR) << "Cannot open HID device as hidraw device.";
    return;
  }

  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE;

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
    LOG(ERROR) << "Failed to start watching device file.";
  }
}

HidConnectionLinux::~HidConnectionLinux() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Disconnect();
}

void HidConnectionLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(fd, device_file_.GetPlatformFile());

  uint8 buffer[1024] = {0};
  int bytes_read =
      HANDLE_EINTR(read(device_file_.GetPlatformFile(), buffer, 1024));
  if (bytes_read < 0) {
    if (errno == EAGAIN) {
      return;
    }
    Disconnect();
    return;
  }

  PendingHidReport report;
  report.buffer = new net::IOBufferWithSize(bytes_read);
  memcpy(report.buffer->data(), buffer, bytes_read);
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionLinux::OnFileCanWriteWithoutBlocking(int fd) {}

void HidConnectionLinux::Disconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_file_watcher_.StopWatchingFileDescriptor();
  device_file_.Close();
  while (!pending_reads_.empty()) {
    PendingHidRead pending_read = pending_reads_.front();
    pending_reads_.pop();
    pending_read.callback.Run(false, 0);
  }
}

void HidConnectionLinux::Read(scoped_refptr<net::IOBufferWithSize> buffer,
                              const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingHidRead pending_read;
  pending_read.buffer = buffer;
  pending_read.callback = callback;
  pending_reads_.push(pending_read);
  ProcessReadQueue();
}

void HidConnectionLinux::Write(uint8_t report_id,
                               scoped_refptr<net::IOBufferWithSize> buffer,
                               const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If report ID is non-zero, insert it into a new copy of the buffer.
  if (report_id != 0)
    buffer = CopyBufferWithReportId(buffer, report_id);
  int bytes_written = HANDLE_EINTR(
      write(device_file_.GetPlatformFile(), buffer->data(), buffer->size()));
  if (bytes_written < 0) {
    Disconnect();
    callback.Run(false, 0);
  } else {
    callback.Run(true, bytes_written);
  }
}

void HidConnectionLinux::GetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer->size() == 0) {
    callback.Run(false, 0);
    return;
  }

  // The first byte of the destination buffer is the report ID being requested.
  buffer->data()[0] = report_id;
  int result = ioctl(device_file_.GetPlatformFile(),
                     HIDIOCGFEATURE(buffer->size()),
                     buffer->data());
  if (result < 0)
    callback.Run(false, 0);
  else
    callback.Run(true, result);
}

void HidConnectionLinux::SendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (report_id != 0)
    buffer = CopyBufferWithReportId(buffer, report_id);
  int result = ioctl(device_file_.GetPlatformFile(),
                     HIDIOCSFEATURE(buffer->size()),
                     buffer->data());
  if (result < 0)
    callback.Run(false, 0);
  else
    callback.Run(true, result);
}

void HidConnectionLinux::ProcessReadQueue() {
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    pending_reads_.pop();
    PendingHidReport report = pending_reports_.front();
    if (report.buffer->size() > read.buffer->size()) {
      read.callback.Run(false, report.buffer->size());
    } else {
      memcpy(read.buffer->data(), report.buffer->data(), report.buffer->size());
      pending_reports_.pop();
      read.callback.Run(true, report.buffer->size());
    }
  }
}

bool HidConnectionLinux::FindHidrawDevNode(udev_device* parent,
                                           std::string* result) {
  udev* udev = udev_device_get_udev(parent);
  if (!udev) {
      return false;
  }
  ScopedUdevEnumeratePtr enumerate(udev_enumerate_new(udev));
  if (!enumerate) {
    return false;
  }
  if (udev_enumerate_add_match_subsystem(enumerate.get(), kHidrawSubsystem)) {
    return false;
  }
  if (udev_enumerate_scan_devices(enumerate.get())) {
    return false;
  }
  std::string parent_path(udev_device_get_devpath(parent));
  if (parent_path.length() == 0 || *parent_path.rbegin() != '/')
    parent_path += '/';
  udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate.get());
  for (udev_list_entry* i = devices; i != NULL;
       i = udev_list_entry_get_next(i)) {
    ScopedUdevDevicePtr hid_dev(
        udev_device_new_from_syspath(udev, udev_list_entry_get_name(i)));
    const char* raw_path = udev_device_get_devnode(hid_dev.get());
    std::string device_path = udev_device_get_devpath(hid_dev.get());
    if (raw_path &&
        !device_path.compare(0, parent_path.length(), parent_path)) {
      std::string sub_path = device_path.substr(parent_path.length());
      if (sub_path.substr(0, sizeof(kHidrawSubsystem)-1) == kHidrawSubsystem) {
        *result = raw_path;
        return true;
      }
    }
  }

  return false;
}

}  // namespace device

