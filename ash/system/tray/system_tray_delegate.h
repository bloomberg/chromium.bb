// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace ash {
struct IMEInfo;
struct IMEPropertyInfo;

class SystemTray;
class SystemTrayItem;

using IMEInfoList = std::vector<IMEInfo>;
using IMEPropertyInfoList = std::vector<IMEPropertyInfo>;

class NetworkingConfigDelegate;

// SystemTrayDelegate is intended for delegating tasks in the System Tray to the
// application (e.g. Chrome). These tasks should be limited to application
// (browser) specific tasks. For non application specific tasks, where possible,
// components/, chromeos/, device/, etc., code should be used directly.
//
// DEPRECATED: This class is being replaced with SystemTrayController and
// SessionController to support mash/mustash. Add new code to those classes.
class ASH_EXPORT SystemTrayDelegate {
 public:
  SystemTrayDelegate();
  virtual ~SystemTrayDelegate();

  // Called after SystemTray has been instantiated.
  virtual void Initialize();

  // Shows login UI to add other users to this session.
  virtual void ShowUserLogin();

  // Returns the currently selected IME.
  virtual void GetCurrentIME(IMEInfo* info);

  // Returns a list of availble IMEs.
  virtual void GetAvailableIMEList(IMEInfoList* list);

  // Returns a list of properties for the currently selected IME.
  virtual void GetCurrentIMEProperties(IMEPropertyInfoList* list);

  // Returns a non-empty string if IMEs are managed by policy.
  virtual base::string16 GetIMEManagedMessage();

  // Switches to the selected input method.
  virtual void SwitchIME(const std::string& ime_id);

  // Activates an IME property.
  virtual void ActivateIMEProperty(const std::string& key);

  // Returns NetworkingConfigDelegate. May return nullptr.
  virtual NetworkingConfigDelegate* GetNetworkingConfigDelegate() const;

  // Retrieves the session start time. Returns |false| if the time is not set.
  virtual bool GetSessionStartTime(base::TimeTicks* session_start_time);

  // Retrieves the session length limit. Returns |false| if no limit is set.
  virtual bool GetSessionLengthLimit(base::TimeDelta* session_length_limit);

  // The active user has been changed. This will be called when the UI is ready
  // to be switched to the new user.
  // Note: This will happen after SessionObserver::ActiveUserChanged fires.
  virtual void ActiveUserWasChanged();

  // Returns true when the Search key is configured to be treated as Caps Lock.
  virtual bool IsSearchKeyMappedToCapsLock();

  // Creates a system tray item for display rotation lock.
  // TODO(jamescook): Remove this when mus has support for display management
  // and we have a DisplayManager equivalent. See http://crbug.com/548429
  virtual std::unique_ptr<SystemTrayItem> CreateRotationLockTrayItem(
      SystemTray* tray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_DELEGATE_H_
