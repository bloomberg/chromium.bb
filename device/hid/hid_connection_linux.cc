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
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "base/tuple.h"
#include "device/hid/hid_service.h"

// These are already defined in newer versions of linux/hidraw.h.
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#endif
#ifndef HIDIOCGFEATURE
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

namespace device {

HidConnectionLinux::HidConnectionLinux(HidDeviceInfo device_info,
                                       std::string dev_node)
    : HidConnection(device_info) {
  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE;

  base::File device_file(base::FilePath(dev_node), flags);
  if (!device_file.IsValid()) {
    base::File::Error file_error = device_file.error_details();

    if (file_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      VLOG(1) << "Access denied opening device read-write, trying read-only.";

      flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

      device_file = base::File(base::FilePath(dev_node), flags);
    }
  }
  if (!device_file.IsValid()) {
    LOG(ERROR) << "Failed to open '" << dev_node << "': "
        << base::File::ErrorToString(device_file.error_details());
    return;
  }

  if (fcntl(device_file.GetPlatformFile(), F_SETFL,
            fcntl(device_file.GetPlatformFile(), F_GETFL) | O_NONBLOCK)) {
    PLOG(ERROR) << "Failed to set non-blocking flag to device file";
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
  Disconnect();
  Flush();
}

void HidConnectionLinux::PlatformRead(const ReadCallback& callback) {
  PendingHidRead pending_read;
  pending_read.callback = callback;
  pending_reads_.push(pending_read);
  ProcessReadQueue();
}

void HidConnectionLinux::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                       size_t size,
                                       const WriteCallback& callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  const ssize_t bytes_written =
      HANDLE_EINTR(write(device_file_.GetPlatformFile(), buffer->data(), size));
  if (bytes_written < 0) {
    VPLOG(1) << "Write failed";
    Disconnect();
    callback.Run(false);
  } else {
    if (static_cast<size_t>(bytes_written) != size) {
      LOG(WARNING) << "Incomplete HID write: " << bytes_written
                   << " != " << size;
    }
    callback.Run(true);
  }
}

void HidConnectionLinux::PlatformGetFeatureReport(
    uint8_t report_id,
    const ReadCallback& callback) {
  // The first byte of the destination buffer is the report ID being requested
  // and is overwritten by the feature report.
  DCHECK_GT(device_info().max_feature_report_size, 0);
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(device_info().max_feature_report_size));
  buffer->data()[0] = report_id;

  int result = ioctl(device_file_.GetPlatformFile(),
                     HIDIOCGFEATURE(buffer->size()),
                     buffer->data());
  if (result < 0) {
    VPLOG(1) << "Failed to get feature report";
    callback.Run(false, NULL, 0);
  } else {
    callback.Run(true, buffer, result);
  }
}

void HidConnectionLinux::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  int result = ioctl(
      device_file_.GetPlatformFile(), HIDIOCSFEATURE(size), buffer->data());
  if (result < 0) {
    VPLOG(1) << "Failed to send feature report";
    callback.Run(false);
  } else {
    callback.Run(true);
  }
}

void HidConnectionLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(thread_checker().CalledOnValidThread());
  DCHECK_EQ(fd, device_file_.GetPlatformFile());

  size_t expected_report_size = device_info().max_input_report_size + 1;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(expected_report_size));
  char* data = buffer->data();
  if (!device_info().has_report_id) {
    // Linux will not prefix the buffer with a report ID if they are not used
    // by the device.
    data[0] = 0;
    data++;
    expected_report_size--;
  }

  ssize_t bytes_read = HANDLE_EINTR(
      read(device_file_.GetPlatformFile(), data, expected_report_size));
  if (bytes_read < 0) {
    if (errno == EAGAIN) {
      return;
    }
    VPLOG(1) << "Read failed";
    Disconnect();
    return;
  }
  if (!device_info().has_report_id) {
    // Include the byte prepended earlier.
    bytes_read++;
  }

  ProcessInputReport(buffer, bytes_read);
}

void HidConnectionLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

void HidConnectionLinux::Disconnect() {
  DCHECK(thread_checker().CalledOnValidThread());
  device_file_watcher_.StopWatchingFileDescriptor();
  device_file_.Close();

  Flush();
}

void HidConnectionLinux::Flush() {
  while (!pending_reads_.empty()) {
    pending_reads_.front().callback.Run(false, NULL, 0);
    pending_reads_.pop();
  }
}

void HidConnectionLinux::ProcessInputReport(scoped_refptr<net::IOBuffer> buffer,
                                            size_t size) {
  DCHECK(thread_checker().CalledOnValidThread());
  PendingHidReport report;
  report.buffer = buffer;
  report.size = size;
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionLinux::ProcessReadQueue() {
  DCHECK(thread_checker().CalledOnValidThread());
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    PendingHidReport report = pending_reports_.front();

    pending_reports_.pop();
    if (CompleteRead(report.buffer, report.size, read.callback)) {
      pending_reads_.pop();
    }
  }
}

}  // namespace device
