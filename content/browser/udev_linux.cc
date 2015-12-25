// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/udev_linux.h"

#include <stddef.h>

#include "base/message_loop/message_loop.h"

namespace content {

UdevLinux::UdevLinux(const std::vector<UdevMonitorFilter>& filters,
                     const UdevNotificationCallback& callback)
    : udev_(device::udev_new()),
      monitor_(device::udev_monitor_new_from_netlink(udev_.get(), "udev")),
      monitor_fd_(-1),
      callback_(callback) {
  CHECK(udev_);
  CHECK(monitor_);

  for (size_t i = 0; i < filters.size(); ++i) {
    int ret = device::udev_monitor_filter_add_match_subsystem_devtype(
        monitor_.get(), filters[i].subsystem, filters[i].devtype);
    CHECK_EQ(0, ret);
  }

  int ret = device::udev_monitor_enable_receiving(monitor_.get());
  CHECK_EQ(0, ret);
  monitor_fd_ = device::udev_monitor_get_fd(monitor_.get());
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
}

udev* UdevLinux::udev_handle() {
  return udev_.get();
}

void UdevLinux::OnFileCanReadWithoutBlocking(int fd) {
  // Events occur when devices attached to the system are added, removed, or
  // change state. udev_monitor_receive_device() will return a device object
  // representing the device which changed and what type of change occured.
  DCHECK_EQ(monitor_fd_, fd);
  device::ScopedUdevDevicePtr dev(
      device::udev_monitor_receive_device(monitor_.get()));
  if (!dev)
    return;

  callback_.Run(dev.get());
}

void UdevLinux::OnFileCanWriteWithoutBlocking(int fd) {
}

}  // namespace content
