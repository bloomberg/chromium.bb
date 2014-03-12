// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_monitor_mac.h"

#import <QTKit/QTKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "media/video/capture/mac/avfoundation_glue.h"

namespace {

// This class is used to keep track of system devices names and their types.
class DeviceInfo {
 public:
  enum DeviceType {
    kAudio,
    kVideo,
    kMuxed,
    kUnknown,
    kInvalid
  };

  DeviceInfo(std::string unique_id, DeviceType type)
      : unique_id_(unique_id), type_(type) {}

  // Operator== is needed here to use this class in a std::find. A given
  // |unique_id_| always has the same |type_| so for comparison purposes the
  // latter can be safely ignored.
  bool operator==(const DeviceInfo& device) const {
    return unique_id_ == device.unique_id_;
  }

  const std::string& unique_id() const { return unique_id_; }
  DeviceType type() const { return type_; }

 private:
  std::string unique_id_;
  DeviceType type_;
  // Allow generated copy constructor and assignment.
};

// Base abstract class used by DeviceMonitorMac to interact with either a QTKit
// or an AVFoundation implementation of events and notifications.
class DeviceMonitorMacImpl {
 public:
  explicit DeviceMonitorMacImpl(content::DeviceMonitorMac* monitor)
      : monitor_(monitor),
        cached_devices_(),
        device_arrival_(nil),
        device_removal_(nil) {
    DCHECK(monitor);
    // Initialise the devices_cache_ with a not-valid entry. For the case in
    // which there is one single device in the system and we get notified when
    // it gets removed, this will prevent the system from thinking that no
    // devices were added nor removed and not notifying the |monitor_|.
    cached_devices_.push_back(DeviceInfo("invalid", DeviceInfo::kInvalid));
  }
  virtual ~DeviceMonitorMacImpl() {}

  virtual void OnDeviceChanged() = 0;

  // Method called by the default notification center when a device is removed
  // or added to the system. It will compare the |cached_devices_| with the
  // current situation, update it, and, if there's an update, signal to
  // |monitor_| with the appropriate device type.
  void ConsolidateDevicesListAndNotify(
      const std::vector<DeviceInfo>& snapshot_devices);

 protected:
  content::DeviceMonitorMac* monitor_;
  std::vector<DeviceInfo> cached_devices_;

  // Handles to NSNotificationCenter block observers.
  id device_arrival_;
  id device_removal_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMacImpl);
};

void DeviceMonitorMacImpl::ConsolidateDevicesListAndNotify(
    const std::vector<DeviceInfo>& snapshot_devices) {
  bool video_device_added = false;
  bool audio_device_added = false;
  bool video_device_removed = false;
  bool audio_device_removed = false;

  // Compare the current system devices snapshot with the ones cached to detect
  // additions, present in the former but not in the latter. If we find a device
  // in snapshot_devices entry also present in cached_devices, we remove it from
  // the latter vector.
  std::vector<DeviceInfo>::const_iterator it;
  for (it = snapshot_devices.begin(); it != snapshot_devices.end(); ++it) {
    std::vector<DeviceInfo>::iterator cached_devices_iterator =
        std::find(cached_devices_.begin(), cached_devices_.end(), *it);
    if (cached_devices_iterator == cached_devices_.end()) {
      video_device_added |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed));
      audio_device_added |= ((it->type() == DeviceInfo::kAudio) ||
                             (it->type() == DeviceInfo::kMuxed));
      DVLOG(1) << "Device has been added, id: " << it->unique_id();
    } else {
      cached_devices_.erase(cached_devices_iterator);
    }
  }
  // All the remaining entries in cached_devices are removed devices.
  for (it = cached_devices_.begin(); it != cached_devices_.end(); ++it) {
    video_device_removed |= ((it->type() == DeviceInfo::kVideo) ||
                             (it->type() == DeviceInfo::kMuxed) ||
                             (it->type() == DeviceInfo::kInvalid));
    audio_device_removed |= ((it->type() == DeviceInfo::kAudio) ||
                             (it->type() == DeviceInfo::kMuxed) ||
                             (it->type() == DeviceInfo::kInvalid));
    DVLOG(1) << "Device has been removed, id: " << it->unique_id();
  }
  // Update the cached devices with the current system snapshot.
  cached_devices_ = snapshot_devices;

  if (video_device_added || video_device_removed)
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
  if (audio_device_added || audio_device_removed)
    monitor_->NotifyDeviceChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);
}

class QTKitMonitorImpl : public DeviceMonitorMacImpl {
 public:
  explicit QTKitMonitorImpl(content::DeviceMonitorMac* monitor);
  virtual ~QTKitMonitorImpl();

  virtual void OnDeviceChanged() OVERRIDE;
 private:
  void CountDevices();
  void OnAttributeChanged(NSNotification* notification);

  id device_change_;
};

QTKitMonitorImpl::QTKitMonitorImpl(content::DeviceMonitorMac* monitor)
    : DeviceMonitorMacImpl(monitor) {
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
  device_change_ =
      [nc addObserverForName:QTCaptureDeviceAttributeDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnAttributeChanged(notification);}];
}

QTKitMonitorImpl::~QTKitMonitorImpl() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
  [nc removeObserver:device_change_];
}

void QTKitMonitorImpl::OnAttributeChanged(
    NSNotification* notification) {
  if ([[[notification userInfo]
         objectForKey:QTCaptureDeviceChangedAttributeKey]
      isEqualToString:QTCaptureDeviceSuspendedAttribute]) {
    OnDeviceChanged();
  }
}

void QTKitMonitorImpl::OnDeviceChanged() {
  std::vector<DeviceInfo> snapshot_devices;

  NSArray* devices = [QTCaptureDevice inputDevices];
  for (QTCaptureDevice* device in devices) {
    DeviceInfo::DeviceType device_type = DeviceInfo::kUnknown;
    // Act as if suspended video capture devices are not attached.  For
    // example, a laptop's internal webcam is suspended when the lid is closed.
    if ([device hasMediaType:QTMediaTypeVideo] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kVideo;
    } else if ([device hasMediaType:QTMediaTypeMuxed] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kMuxed;
    } else if ([device hasMediaType:QTMediaTypeSound] &&
        ![[device attributeForKey:QTCaptureDeviceSuspendedAttribute]
        boolValue]) {
      device_type = DeviceInfo::kAudio;
    }
    snapshot_devices.push_back(
        DeviceInfo([[device uniqueID] UTF8String], device_type));
  }
  ConsolidateDevicesListAndNotify(snapshot_devices);
}

// Forward declaration for use by CrAVFoundationDeviceObserver.
class AVFoundationMonitorImpl;

}  // namespace

// This class is a Key-Value Observer (KVO) shim.  It is needed because C++
// classes cannot observe Key-Values directly.
@interface CrAVFoundationDeviceObserver : NSObject {
 @private
  AVFoundationMonitorImpl* receiver_;
}

- (id)initWithChangeReceiver:(AVFoundationMonitorImpl*)receiver;
- (void)startObserving:(CrAVCaptureDevice*)device;
- (void)stopObserving:(CrAVCaptureDevice*)device;

@end

namespace {

class AVFoundationMonitorImpl : public DeviceMonitorMacImpl {
 public:
  explicit AVFoundationMonitorImpl(content::DeviceMonitorMac* monitor);
  virtual ~AVFoundationMonitorImpl();

  virtual void OnDeviceChanged() OVERRIDE;

 private:
  base::scoped_nsobject<CrAVFoundationDeviceObserver> suspend_observer_;
  DISALLOW_COPY_AND_ASSIGN(AVFoundationMonitorImpl);
};

AVFoundationMonitorImpl::AVFoundationMonitorImpl(
    content::DeviceMonitorMac* monitor)
    : DeviceMonitorMacImpl(monitor) {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  device_arrival_ =
      [nc addObserverForName:AVFoundationGlue::
          AVCaptureDeviceWasConnectedNotification()
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  device_removal_ =
      [nc addObserverForName:AVFoundationGlue::
          AVCaptureDeviceWasDisconnectedNotification()
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                      OnDeviceChanged();}];
  suspend_observer_.reset(
      [[CrAVFoundationDeviceObserver alloc] initWithChangeReceiver:this]);
  for (CrAVCaptureDevice* device in [AVCaptureDeviceGlue devices])
    [suspend_observer_ startObserving:device];
}

AVFoundationMonitorImpl::~AVFoundationMonitorImpl() {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
  for (CrAVCaptureDevice* device in [AVCaptureDeviceGlue devices])
    [suspend_observer_ stopObserving:device];
}

void AVFoundationMonitorImpl::OnDeviceChanged() {
  std::vector<DeviceInfo> snapshot_devices;
  for (CrAVCaptureDevice* device in [AVCaptureDeviceGlue devices]) {
    [suspend_observer_ startObserving:device];
    BOOL suspended = [device respondsToSelector:@selector(isSuspended)] &&
        [device isSuspended];
    DeviceInfo::DeviceType device_type = DeviceInfo::kUnknown;
    if ([device hasMediaType:AVFoundationGlue::AVMediaTypeVideo()]) {
      if (suspended)
        continue;
      device_type = DeviceInfo::kVideo;
    } else if ([device hasMediaType:AVFoundationGlue::AVMediaTypeMuxed()]) {
      device_type = suspended ? DeviceInfo::kAudio : DeviceInfo::kMuxed;
    } else if ([device hasMediaType:AVFoundationGlue::AVMediaTypeAudio()]) {
      device_type = DeviceInfo::kAudio;
    }
    snapshot_devices.push_back(DeviceInfo([[device uniqueID] UTF8String],
                                          device_type));
  }
  ConsolidateDevicesListAndNotify(snapshot_devices);
}

}  // namespace

@implementation CrAVFoundationDeviceObserver

- (id)initWithChangeReceiver:(AVFoundationMonitorImpl*)receiver {
  if ((self = [super init])) {
    DCHECK(receiver != NULL);
    receiver_ = receiver;
  }
  return self;
}

- (void)startObserving:(CrAVCaptureDevice*)device {
  DCHECK(device != nil);
  [device addObserver:self
           forKeyPath:@"suspended"
              options:0
              context:device];
  [device addObserver:self
           forKeyPath:@"connected"
              options:0
              context:device];
}

- (void)stopObserving:(CrAVCaptureDevice*)device {
  DCHECK(device != nil);
  [device removeObserver:self
              forKeyPath:@"suspended"];
  [device removeObserver:self
              forKeyPath:@"connected"];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqual:@"suspended"])
    receiver_->OnDeviceChanged();
  if ([keyPath isEqual:@"connected"])
    [self stopObserving:static_cast<CrAVCaptureDevice*>(context)];
}

@end  // @implementation CrAVFoundationDeviceObserver

namespace content {

DeviceMonitorMac::DeviceMonitorMac() {
  // Both QTKit and AVFoundation do not need to be fired up until the user
  // exercises a GetUserMedia. Bringing up either library and enumerating the
  // devices in the system is an operation taking in the range of hundred of ms,
  // so it is triggered explicitly from MediaStreamManager::StartMonitoring().
}

DeviceMonitorMac::~DeviceMonitorMac() {}

void DeviceMonitorMac::StartMonitoring() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "Monitoring via AVFoundation";
    device_monitor_impl_.reset(new AVFoundationMonitorImpl(this));
  } else {
    DVLOG(1) << "Monitoring via QTKit";
    device_monitor_impl_.reset(new QTKitMonitorImpl(this));
  }
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace content
