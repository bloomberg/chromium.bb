// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_monitor_mac.h"

#import <QTKit/QTKit.h>

#include "base/logging.h"

namespace content {

class DeviceMonitorMac::QTMonitorImpl {
 public:
  explicit QTMonitorImpl(DeviceMonitorMac* monitor);
  virtual ~QTMonitorImpl() {}

  void Start();
  void Stop();

 private:
  void OnDeviceChanged();

  DeviceMonitorMac* monitor_;
  int number_audio_devices_;
  int number_video_devices_;
  id device_arrival_;
  id device_removal_;

  DISALLOW_COPY_AND_ASSIGN(QTMonitorImpl);
};

DeviceMonitorMac::QTMonitorImpl::QTMonitorImpl(DeviceMonitorMac* monitor)
    : monitor_(monitor),
      number_audio_devices_(0),
      number_video_devices_(0),
      device_arrival_(nil),
      device_removal_(nil) {
  DCHECK(monitor);
}

void DeviceMonitorMac::QTMonitorImpl::Start() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  device_arrival_ =
    [nc addObserverForName:QTCaptureDeviceWasConnectedNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                    OnDeviceChanged();}];

  device_removal_ =
      [nc addObserverForName:QTCaptureDeviceWasDisconnectedNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
}

void DeviceMonitorMac::QTMonitorImpl::Stop() {
  if (!monitor_)
    return;

  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
}

void DeviceMonitorMac::QTMonitorImpl::OnDeviceChanged() {
  NSArray* devices = [QTCaptureDevice inputDevices];
  int number_video_devices = 0;
  int number_audio_devices = 0;
  for (QTCaptureDevice* device in devices) {
    if ([device hasMediaType:QTMediaTypeVideo] ||
        [device hasMediaType:QTMediaTypeMuxed])
      ++number_video_devices;

    if ([device hasMediaType:QTMediaTypeSound] ||
        [device hasMediaType:QTMediaTypeMuxed])
      ++number_audio_devices;
  }

  if (number_video_devices_ != number_video_devices) {
    number_video_devices_ = number_video_devices;
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  }

  if (number_audio_devices_ != number_audio_devices) {
    number_audio_devices_ = number_audio_devices;
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);
  }
}

DeviceMonitorMac::DeviceMonitorMac() {
  qt_monitor_.reset(new QTMonitorImpl(this));
  qt_monitor_->Start();
}

DeviceMonitorMac::~DeviceMonitorMac() {
  qt_monitor_->Stop();
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace content
