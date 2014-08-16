// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager_win.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/win/message_window.h"
#include "base/win/windows_version.h"
#include "content/browser/battery_status/battery_status_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {

typedef BatteryStatusService::BatteryUpdateCallback BatteryCallback;

const wchar_t kWindowClassName[] = L"BatteryStatusMessageWindow";

// Message-only window for handling battery changes on Windows.
class BatteryStatusObserver
    : public base::RefCountedThreadSafe<BatteryStatusObserver> {
 public:
  explicit BatteryStatusObserver(const BatteryCallback& callback)
      : power_handle_(NULL),
        battery_change_handle_(NULL),
        callback_(callback) {
  }

  virtual ~BatteryStatusObserver() { DCHECK(!window_); }

  void Start() {
    // Need to start on the UI thread to receive battery status notifications.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&BatteryStatusObserver::StartOnUI, this));
  }

  void Stop() {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&BatteryStatusObserver::StopOnUI, this));
  }

 private:
  void StartOnUI() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (window_)
      return;

    if (CreateMessageWindow()) {
      BatteryChanged();
      // RegisterPowerSettingNotification function work from Windows Vista
      // onwards. However even without them we will receive notifications,
      // e.g. when a power source is connected.
      // TODO(timvolodine) : consider polling for battery changes on windows
      // versions prior to Vista, see crbug.com/402466.
      power_handle_ =
          RegisterNotification(&GUID_ACDC_POWER_SOURCE);
      battery_change_handle_ =
          RegisterNotification(&GUID_BATTERY_PERCENTAGE_REMAINING);
    } else {
      // Could not create a message window, execute callback with the default
      // values.
      callback_.Run(blink::WebBatteryStatus());
    }
  }

  void StopOnUI() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!window_)
      return;

    if (power_handle_)
      UnregisterNotification(power_handle_);
    if (battery_change_handle_)
      UnregisterNotification(battery_change_handle_);
    window_.reset();
  }

  void BatteryChanged() {
    SYSTEM_POWER_STATUS win_status;
    if (GetSystemPowerStatus(&win_status))
      callback_.Run(ComputeWebBatteryStatus(win_status));
    else
      callback_.Run(blink::WebBatteryStatus());
  }

  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result) {
    switch(message) {
      case WM_POWERBROADCAST:
        if (wparam == PBT_APMPOWERSTATUSCHANGE ||
            wparam == PBT_POWERSETTINGCHANGE) {
          BatteryChanged();
        }
        *result = NULL;
        return true;
      default:
        return false;
    }
  }

  HPOWERNOTIFY RegisterNotification(LPCGUID power_setting) {
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      return NULL;

    return RegisterPowerSettingNotification(window_->hwnd(), power_setting,
                                     DEVICE_NOTIFY_WINDOW_HANDLE);
  }

  BOOL UnregisterNotification(HPOWERNOTIFY handle) {
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      return FALSE;

    return UnregisterPowerSettingNotification(handle);
  }

  bool CreateMessageWindow() {
    // TODO(timvolodine): consider reusing the message window of PowerMonitor.
    window_.reset(new base::win::MessageWindow());
    if (!window_->CreateNamed(base::Bind(&BatteryStatusObserver::HandleMessage,
                                         base::Unretained(this)),
                              base::string16(kWindowClassName))) {
      LOG(ERROR) << "Failed to create message window: " << kWindowClassName;
      window_.reset();
      return false;
    }
    return true;
  }

  HPOWERNOTIFY power_handle_;
  HPOWERNOTIFY battery_change_handle_;
  BatteryCallback callback_;
  scoped_ptr<base::win::MessageWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusObserver);
};

class BatteryStatusManagerWin : public BatteryStatusManager {
 public:
  explicit BatteryStatusManagerWin(const BatteryCallback& callback)
      : battery_observer_(new BatteryStatusObserver(callback)) {}
  virtual ~BatteryStatusManagerWin() { battery_observer_->Stop(); }

 public:
  // BatteryStatusManager:
  virtual bool StartListeningBatteryChange() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    battery_observer_->Start();
    return true;
  }

  virtual void StopListeningBatteryChange() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    battery_observer_->Stop();
  }

 private:
  scoped_refptr<BatteryStatusObserver> battery_observer_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusManagerWin);
};

}  // namespace

blink::WebBatteryStatus ComputeWebBatteryStatus(
    const SYSTEM_POWER_STATUS& win_status) {
  blink::WebBatteryStatus status;
  status.charging = win_status.ACLineStatus != WIN_AC_LINE_STATUS_OFFLINE;

  // Set level if available. Otherwise keep the default value which is 1.
  if (win_status.BatteryLifePercent != 255) {
    // Convert percentage to a value between 0 and 1 with 2 significant digits.
    status.level = static_cast<double>(win_status.BatteryLifePercent) / 100.;
  }

  if (!status.charging) {
    // Set dischargingTime if available otherwise keep the default value,
    // which is +Infinity.
    if (win_status.BatteryLifeTime != (DWORD)-1)
      status.dischargingTime = win_status.BatteryLifeTime;
    status.chargingTime = std::numeric_limits<double>::infinity();
  } else {
    // Set chargingTime to +Infinity if not fully charged, otherwise leave the
    // default value, which is 0.
    if (status.level < 1)
      status.chargingTime = std::numeric_limits<double>::infinity();
  }
  return status;
}

// static
scoped_ptr<BatteryStatusManager> BatteryStatusManager::Create(
    const BatteryStatusService::BatteryUpdateCallback& callback) {
  return scoped_ptr<BatteryStatusManager>(
      new BatteryStatusManagerWin(callback));
}

}  // namespace content
