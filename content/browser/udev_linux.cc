// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/udev_linux.h"

#include <libudev.h>

#include "base/message_loop/message_loop.h"

namespace content {

UdevLinux::UdevLinux(const std::vector<UdevMonitorFilter>& filters,
                     const UdevNotificationCallback& callback)
    : udev_(udev_new()),
      monitor_(NULL),
      monitor_fd_(-1),
      callback_(callback) {
  CHECK(udev_);

  monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  CHECK(monitor_);

  for (size_t i = 0; i < filters.size(); ++i) {
    int ret = udev_monitor_filter_add_match_subsystem_devtype(
        monitor_, filters[i].subsystem, filters[i].devtype);
    CHECK_EQ(0, ret);
  }

  int ret = udev_monitor_enable_receiving(monitor_);
  CHECK_EQ(0, ret);
  monitor_fd_ = udev_monitor_get_fd(monitor_);
  CHECK_GE(monitor_fd_, 0);

  bool success = base::MessageLoopForIO::current()->WatchFileDescriptor(
      monitor_fd_,
      true,
      base::MessageLoopForIO::WATCH_READ,
      &monitor_watcher_,
      this);
  CHECK(success);
}

UdevLinux::~UdevLinux() {
  monitor_watcher_.StopWatchingFileDescriptor();
  udev_monitor_unref(monitor_);
  udev_unref(udev_);
}

udev* UdevLinux::udev_handle() {
  return udev_;
}

void UdevLinux::OnFileCanReadWithoutBlocking(int fd) {
  // Events occur when devices attached to the system are added, removed, or
  // change state. udev_monitor_receive_device() will return a device object
  // representing the device which changed and what type of change occured.
  DCHECK_EQ(monitor_fd_, fd);
  udev_device* dev = udev_monitor_receive_device(monitor_);
  if (!dev)
    return;

  callback_.Run(dev);
  udev_device_unref(dev);
}

void UdevLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

}  // namespace content
