// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/public/platform/WebBatteryStatus.h"

namespace content {

namespace {

typedef BatteryStatusService::BatteryUpdateCallback BatteryCallback;

// Returns the value corresponding to |key| in the dictionary |description|.
// Returns |default_value| if the dictionary does not contain |key|, the
// corresponding value is NULL or it could not be converted to SInt64.
SInt64 GetValueAsSInt64(CFDictionaryRef description,
                        CFStringRef key,
                        SInt64 default_value) {
  CFNumberRef number =
      base::mac::GetValueFromDictionary<CFNumberRef>(description, key);
  SInt64 value;

  if (number && CFNumberGetValue(number, kCFNumberSInt64Type, &value))
    return value;

  return default_value;
}

bool GetValueAsBoolean(CFDictionaryRef description,
                       CFStringRef key,
                       bool default_value) {
  CFBooleanRef boolean =
      base::mac::GetValueFromDictionary<CFBooleanRef>(description, key);

  return boolean ? CFBooleanGetValue(boolean) : default_value;
}

bool CFStringsAreEqual(CFStringRef string1, CFStringRef string2) {
  if (!string1 || !string2)
    return false;
  return CFStringCompare(string1, string2, 0) == kCFCompareEqualTo;
}

void UpdateNumberBatteriesHistogram(int count) {
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "BatteryStatus.NumberBatteriesMac", count, 1, 5, 6);
}

void FetchBatteryStatus(CFDictionaryRef description,
                        blink::WebBatteryStatus& status) {
  CFStringRef current_state =
      base::mac::GetValueFromDictionary<CFStringRef>(description,
          CFSTR(kIOPSPowerSourceStateKey));

  bool on_battery_power =
      CFStringsAreEqual(current_state, CFSTR(kIOPSBatteryPowerValue));
  bool is_charging =
      GetValueAsBoolean(description, CFSTR(kIOPSIsChargingKey), true);
  bool is_charged =
      GetValueAsBoolean(description, CFSTR(kIOPSIsChargedKey), false);

  status.charging = !on_battery_power || is_charging;

  SInt64 current_capacity =
      GetValueAsSInt64(description, CFSTR(kIOPSCurrentCapacityKey), -1);
  SInt64 max_capacity =
      GetValueAsSInt64(description, CFSTR(kIOPSMaxCapacityKey), -1);

  // Set level if it is available and valid. Otherwise leave the default value,
  // which is 1.
  if (current_capacity != -1 && max_capacity != -1 &&
      current_capacity <= max_capacity && max_capacity != 0) {
    status.level = current_capacity / static_cast<double>(max_capacity);
  }

  if (is_charging) {
    SInt64 charging_time =
        GetValueAsSInt64(description, CFSTR(kIOPSTimeToFullChargeKey), -1);

    // Battery is charging: set the charging time if it's available, otherwise
    // set to +infinity.
    status.chargingTime = charging_time != -1
        ? base::TimeDelta::FromMinutes(charging_time).InSeconds()
        : std::numeric_limits<double>::infinity();
  } else {
    // Battery is not charging.
    // Set chargingTime to +infinity if the battery is not charged. Otherwise
    // leave the default value, which is 0.
    if (!is_charged)
      status.chargingTime = std::numeric_limits<double>::infinity();

    // Set dischargingTime if it's available and valid, i.e. when on battery
    // power. Otherwise leave the default value, which is +infinity.
    if (on_battery_power) {
      SInt64 discharging_time =
          GetValueAsSInt64(description, CFSTR(kIOPSTimeToEmptyKey), -1);
      if (discharging_time != -1) {
        status.dischargingTime =
            base::TimeDelta::FromMinutes(discharging_time).InSeconds();
      }
    }
  }
}

std::vector<blink::WebBatteryStatus> GetInternalBatteriesStates() {
  std::vector<blink::WebBatteryStatus> internal_sources;

  base::ScopedCFTypeRef<CFTypeRef> info(IOPSCopyPowerSourcesInfo());
  base::ScopedCFTypeRef<CFArrayRef> power_sources_list(
      IOPSCopyPowerSourcesList(info));
  CFIndex count = CFArrayGetCount(power_sources_list);

  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef description = IOPSGetPowerSourceDescription(info,
        CFArrayGetValueAtIndex(power_sources_list, i));

    if (!description)
      continue;

    CFStringRef transport_type =
        base::mac::GetValueFromDictionary<CFStringRef>(description,
            CFSTR(kIOPSTransportTypeKey));

    bool internal_source =
        CFStringsAreEqual(transport_type, CFSTR(kIOPSInternalType));
    bool source_present =
        GetValueAsBoolean(description, CFSTR(kIOPSIsPresentKey), false);

    if (internal_source && source_present) {
      blink::WebBatteryStatus status;
      FetchBatteryStatus(description, status);
      internal_sources.push_back(status);
    }
  }

  return internal_sources;
}

void OnBatteryStatusChanged(const BatteryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::vector<blink::WebBatteryStatus> batteries(GetInternalBatteriesStates());

  if (batteries.empty()) {
    callback.Run(blink::WebBatteryStatus());
    return;
  }

  // TODO(timvolodine): implement the case when there are multiple internal
  // sources, e.g. when multiple batteries are present. Currently this will
  // fail a DCHECK.
  DCHECK(batteries.size() == 1);
  callback.Run(batteries.front());
}

class BatteryStatusObserver
    : public base::RefCountedThreadSafe<BatteryStatusObserver> {
 public:
  explicit BatteryStatusObserver(const BatteryCallback& callback)
      : callback_(callback) {}

  void Start() {
    // Need to start on a thread with UI-type message loop for
    // |notifier_run_loop_| to receive callbacks.
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      StartOnUI();
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&BatteryStatusObserver::StartOnUI, this));
    }
  }

  void Stop() {
    if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      StopOnUI();
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&BatteryStatusObserver::StopOnUI, this));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<BatteryStatusObserver>;
  virtual ~BatteryStatusObserver() { DCHECK(!notifier_run_loop_source_); }

  void StartOnUI() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (notifier_run_loop_source_)
      return;

    notifier_run_loop_source_.reset(
        IOPSNotificationCreateRunLoopSource(OnBatteryStatusChangedUI,
                                            static_cast<void*>(&callback_)));
    if (!notifier_run_loop_source_) {
      LOG(ERROR) << "Failed to create battery status notification run loop";
      // Make sure to execute to callback with the default values.
      callback_.Run(blink::WebBatteryStatus());
      return;
    }

    OnBatteryStatusChangedUI(static_cast<void*>(&callback_));
    CFRunLoopAddSource(CFRunLoopGetCurrent(), notifier_run_loop_source_,
                       kCFRunLoopDefaultMode);
    UpdateNumberBatteriesHistogram(GetInternalBatteriesStates().size());
  }

  void StopOnUI() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    if (!notifier_run_loop_source_)
      return;

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), notifier_run_loop_source_,
                          kCFRunLoopDefaultMode);
    notifier_run_loop_source_.reset();
  }

  static void OnBatteryStatusChangedUI(void* callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Offload fetching of values and callback execution to the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&OnBatteryStatusChanged,
                   *static_cast<BatteryCallback*>(callback)));
  }

  BatteryCallback callback_;
  base::ScopedCFTypeRef<CFRunLoopSourceRef> notifier_run_loop_source_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusObserver);
};

class BatteryStatusManagerMac : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerMac(const BatteryCallback& callback)
      : notifier_(new BatteryStatusObserver(callback)) {}

  virtual ~BatteryStatusManagerMac() {
    notifier_->Stop();
  }

  // BatteryStatusManager:
  virtual bool StartListeningBatteryChange() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    notifier_->Start();
    return true;
  }

  virtual void StopListeningBatteryChange() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    notifier_->Stop();
  }

 private:
  scoped_refptr<BatteryStatusObserver> notifier_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerMac);
};

} // end namespace

// static
scoped_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return scoped_ptr<BatteryStatusManager>(
      new BatteryStatusManagerMac(callback));
}

}  // namespace content
