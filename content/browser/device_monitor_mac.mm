// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_monitor_mac.h"

#import <QTKit/QTKit.h>

#include <set>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_thread.h"
#import "media/video/capture/mac/avfoundation_glue.h"

using content::BrowserThread;

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
class SuspendObserverDelegate;

}  // namespace

// This class is a Key-Value Observer (KVO) shim. It is needed because C++
// classes cannot observe Key-Values directly. Created, manipulated, and
// destroyed on the UI Thread by SuspendObserverDelegate.
@interface CrAVFoundationDeviceObserver : NSObject {
 @private
  // Callback for device changed, has to run on Device Thread.
  base::Closure onDeviceChangedCallback_;

  // Member to keep track of the devices we are already monitoring.
  std::set<base::scoped_nsobject<CrAVCaptureDevice> > monitoredDevices_;
}

- (id)initWithOnChangedCallback:(const base::Closure&)callback;
- (void)startObserving:(base::scoped_nsobject<CrAVCaptureDevice>)device;
- (void)stopObserving:(CrAVCaptureDevice*)device;
- (void)clearOnDeviceChangedCallback;

@end

namespace {

// This class owns and manages the lifetime of a CrAVFoundationDeviceObserver.
// It is created and destroyed in UI thread by AVFoundationMonitorImpl, and it
// operates on this thread except for the expensive device enumerations which
// are run on Device Thread.
class SuspendObserverDelegate :
    public base::RefCountedThreadSafe<SuspendObserverDelegate> {
 public:
  explicit SuspendObserverDelegate(DeviceMonitorMacImpl* monitor);

  // Create |suspend_observer_| for all devices and register OnDeviceChanged()
  // as its change callback. Schedule bottom half in DoStartObserver().
  void StartObserver(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread);
  // Enumerate devices in |device_thread| and run the bottom half in
  // DoOnDeviceChange(). |suspend_observer_| calls back here on suspend event,
  // and our parent AVFoundationMonitorImpl calls on connect/disconnect device.
  void OnDeviceChanged(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread);
  // Remove the device monitor's weak reference. Remove ourselves as suspend
  // notification observer from |suspend_observer_|.
  void ResetDeviceMonitor();

 private:
  friend class base::RefCountedThreadSafe<SuspendObserverDelegate>;

  virtual ~SuspendObserverDelegate();

  // Bottom half of StartObserver(), starts |suspend_observer_| for all devices.
  // Assumes that |devices| has been retained prior to being called, and
  // releases it internally.
  void DoStartObserver(NSArray* devices);
  // Bottom half of OnDeviceChanged(), starts |suspend_observer_| for current
  // devices and composes a snapshot of them to send it to
  // |avfoundation_monitor_impl_|. Assumes that |devices| has been retained
  // prior to being called, and releases it internally.
  void DoOnDeviceChanged(NSArray* devices);

  base::scoped_nsobject<CrAVFoundationDeviceObserver> suspend_observer_;
  DeviceMonitorMacImpl* avfoundation_monitor_impl_;
};

SuspendObserverDelegate::SuspendObserverDelegate(DeviceMonitorMacImpl* monitor)
    : avfoundation_monitor_impl_(monitor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SuspendObserverDelegate::StartObserver(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::Closure on_device_changed_callback =
      base::Bind(&SuspendObserverDelegate::OnDeviceChanged,
                 this, device_thread);
  suspend_observer_.reset([[CrAVFoundationDeviceObserver alloc]
      initWithOnChangedCallback:on_device_changed_callback]);

  // Enumerate the devices in Device thread and post the observers start to be
  // done on UI thread. The devices array is retained in |device_thread| and
  // released in DoStartObserver().
  base::PostTaskAndReplyWithResult(device_thread, FROM_HERE,
      base::BindBlock(^{ return [[AVCaptureDeviceGlue devices] retain]; }),
      base::Bind(&SuspendObserverDelegate::DoStartObserver, this));
}

void SuspendObserverDelegate::OnDeviceChanged(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Enumerate the devices in Device thread and post the consolidation of the
  // new devices and the old ones to be done on UI thread. The devices array
  // is retained in |device_thread| and released in DoOnDeviceChanged().
  PostTaskAndReplyWithResult(device_thread, FROM_HERE,
      base::BindBlock(^{ return [[AVCaptureDeviceGlue devices] retain]; }),
      base::Bind(&SuspendObserverDelegate::DoOnDeviceChanged, this));
}

void SuspendObserverDelegate::ResetDeviceMonitor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  avfoundation_monitor_impl_ = NULL;
  [suspend_observer_ clearOnDeviceChangedCallback];
}

SuspendObserverDelegate::~SuspendObserverDelegate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SuspendObserverDelegate::DoStartObserver(NSArray* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::scoped_nsobject<NSArray> auto_release(devices);
  for (CrAVCaptureDevice* device in devices) {
    base::scoped_nsobject<CrAVCaptureDevice> device_ptr([device retain]);
    [suspend_observer_ startObserving:device_ptr];
  }
}

void SuspendObserverDelegate::DoOnDeviceChanged(NSArray* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::scoped_nsobject<NSArray> auto_release(devices);
  std::vector<DeviceInfo> snapshot_devices;
  for (CrAVCaptureDevice* device in devices) {
    base::scoped_nsobject<CrAVCaptureDevice> device_ptr([device retain]);
    [suspend_observer_ startObserving:device_ptr];

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

  // |avfoundation_monitor_impl_| might have been NULLed asynchronously before
  // arriving at this line.
  if (avfoundation_monitor_impl_) {
    avfoundation_monitor_impl_->ConsolidateDevicesListAndNotify(
        snapshot_devices);
  }
}

// AVFoundation implementation of the Mac Device Monitor, registers as a global
// device connect/disconnect observer and plugs suspend/wake up device observers
// per device. This class is created and lives in UI thread. Owns a
// SuspendObserverDelegate that notifies when a device is suspended/resumed.
class AVFoundationMonitorImpl : public DeviceMonitorMacImpl {
 public:
  AVFoundationMonitorImpl(
      content::DeviceMonitorMac* monitor,
      const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner);
  virtual ~AVFoundationMonitorImpl();

  virtual void OnDeviceChanged() OVERRIDE;

 private:
  // {Video,AudioInput}DeviceManager's "Device" thread task runner used for
  // posting tasks to |suspend_observer_delegate_|; valid after
  // MediaStreamManager calls StartMonitoring().
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  scoped_refptr<SuspendObserverDelegate> suspend_observer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationMonitorImpl);
};

AVFoundationMonitorImpl::AVFoundationMonitorImpl(
    content::DeviceMonitorMac* monitor,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner)
    : DeviceMonitorMacImpl(monitor),
      device_task_runner_(device_task_runner),
      suspend_observer_delegate_(new SuspendObserverDelegate(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  suspend_observer_delegate_->StartObserver(device_task_runner_);
}

AVFoundationMonitorImpl::~AVFoundationMonitorImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  suspend_observer_delegate_->ResetDeviceMonitor();
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:device_arrival_];
  [nc removeObserver:device_removal_];
}

void AVFoundationMonitorImpl::OnDeviceChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  suspend_observer_delegate_->OnDeviceChanged(device_task_runner_);
}

}  // namespace

@implementation CrAVFoundationDeviceObserver

- (id)initWithOnChangedCallback:(const base::Closure&)callback {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if ((self = [super init])) {
    DCHECK(!callback.is_null());
    onDeviceChangedCallback_ = callback;
  }
  return self;
}

- (void)dealloc {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<base::scoped_nsobject<CrAVCaptureDevice> >::iterator it =
      monitoredDevices_.begin();
  while (it != monitoredDevices_.end())
    [self removeObservers:*(it++)];
  [super dealloc];
}

- (void)startObserving:(base::scoped_nsobject<CrAVCaptureDevice>)device {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device != nil);
  // Skip this device if there are already observers connected to it.
  if (std::find(monitoredDevices_.begin(), monitoredDevices_.end(), device) !=
      monitoredDevices_.end()) {
    return;
  }
  [device addObserver:self
           forKeyPath:@"suspended"
              options:0
              context:device.get()];
  [device addObserver:self
           forKeyPath:@"connected"
              options:0
              context:device.get()];
  monitoredDevices_.insert(device);
}

- (void)stopObserving:(CrAVCaptureDevice*)device {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(device != nil);

  std::set<base::scoped_nsobject<CrAVCaptureDevice> >::iterator found =
      std::find(monitoredDevices_.begin(), monitoredDevices_.end(), device);
  DCHECK(found != monitoredDevices_.end());
  [self removeObservers:*found];
  monitoredDevices_.erase(found);
}

- (void)clearOnDeviceChangedCallback {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  onDeviceChangedCallback_.Reset();
}

- (void)removeObservers:(CrAVCaptureDevice*)device {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Check sanity of |device| via its -observationInfo. http://crbug.com/371271.
  if ([device observationInfo]) {
    [device removeObserver:self
                forKeyPath:@"suspended"];
    [device removeObserver:self
                forKeyPath:@"connected"];
  }
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if ([keyPath isEqual:@"suspended"])
    onDeviceChangedCallback_.Run();
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

void DeviceMonitorMac::StartMonitoring(
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (AVFoundationGlue::IsAVFoundationSupported()) {
    DVLOG(1) << "Monitoring via AVFoundation";
    device_monitor_impl_.reset(new AVFoundationMonitorImpl(this,
                                                           device_task_runner));
  } else {
    DVLOG(1) << "Monitoring via QTKit";
    device_monitor_impl_.reset(new QTKitMonitorImpl(this));
  }
}

void DeviceMonitorMac::NotifyDeviceChanged(
    base::SystemMonitor::DeviceType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(xians): Remove the global variable for SystemMonitor.
  base::SystemMonitor::Get()->ProcessDevicesChanged(type);
}

}  // namespace content
