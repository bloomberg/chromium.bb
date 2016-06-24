// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/system/user/user_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/bluetooth/bluetooth_observer.h"
#include "ash/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/network/network_portal_detector_observer.h"
#include "ash/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/system/chromeos/screen_security/screen_share_observer.h"
#include "ash/system/chromeos/session/last_window_closed_observer.h"
#include "ash/system/chromeos/session/logout_button_observer.h"
#include "ash/system/chromeos/session/session_length_limit_observer.h"
#include "ash/system/tray/media_security/media_capture_observer.h"
#include "base/time/time.h"
#endif

namespace ash {

#if defined(OS_CHROMEOS)
class NetworkStateNotifier;
#endif

class ASH_EXPORT SystemTrayNotifier {
 public:
  SystemTrayNotifier();
  ~SystemTrayNotifier();

  void AddUserObserver(UserObserver* observer);
  void RemoveUserObserver(UserObserver* observer);

#if defined(OS_CHROMEOS)
  void AddBluetoothObserver(BluetoothObserver* observer);
  void RemoveBluetoothObserver(BluetoothObserver* observer);

  void AddLogoutButtonObserver(LogoutButtonObserver* observer);
  void RemoveLogoutButtonObserver(LogoutButtonObserver* observer);

  void AddSessionLengthLimitObserver(SessionLengthLimitObserver* observer);
  void RemoveSessionLengthLimitObserver(SessionLengthLimitObserver* observer);

  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);

  void AddNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);
  void RemoveNetworkPortalDetectorObserver(
      NetworkPortalDetectorObserver* observer);

  void AddEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void RemoveEnterpriseDomainObserver(EnterpriseDomainObserver* observer);

  void AddMediaCaptureObserver(MediaCaptureObserver* observer);
  void RemoveMediaCaptureObserver(MediaCaptureObserver* observer);

  void AddScreenCaptureObserver(ScreenCaptureObserver* observer);
  void RemoveScreenCaptureObserver(ScreenCaptureObserver* observer);

  void AddScreenShareObserver(ScreenShareObserver* observer);
  void RemoveScreenShareObserver(ScreenShareObserver* observer);

  void AddLastWindowClosedObserver(LastWindowClosedObserver* observer);
  void RemoveLastWindowClosedObserver(LastWindowClosedObserver* observer);
#endif

  void NotifyUserUpdate();
  void NotifyUserAddedToSession();
#if defined(OS_CHROMEOS)
  void NotifyRefreshBluetooth();
  void NotifyBluetoothDiscoveringChanged();
  void NotifyShowLoginButtonChanged(bool show_login_button);
  void NotifyLogoutDialogDurationChanged(base::TimeDelta duration);
  void NotifySessionStartTimeChanged();
  void NotifySessionLengthLimitChanged();
  void NotifyRequestToggleWifi();
  void NotifyOnCaptivePortalDetected(const std::string& service_path);
  void NotifyEnterpriseDomainChanged();
  void NotifyMediaCaptureChanged();
  void NotifyScreenCaptureStart(const base::Closure& stop_callback,
                                const base::string16& sharing_app_name);
  void NotifyScreenCaptureStop();
  void NotifyScreenShareStart(const base::Closure& stop_callback,
                              const base::string16& helper_name);
  void NotifyScreenShareStop();
  void NotifyLastWindowClosed();
#endif

 private:
  base::ObserverList<UserObserver> user_observers_;
#if defined(OS_CHROMEOS)
  base::ObserverList<BluetoothObserver> bluetooth_observers_;
  base::ObserverList<LogoutButtonObserver> logout_button_observers_;
  base::ObserverList<SessionLengthLimitObserver>
      session_length_limit_observers_;
  base::ObserverList<NetworkObserver> network_observers_;
  base::ObserverList<NetworkPortalDetectorObserver>
      network_portal_detector_observers_;
  base::ObserverList<EnterpriseDomainObserver> enterprise_domain_observers_;
  base::ObserverList<MediaCaptureObserver> media_capture_observers_;
  base::ObserverList<ScreenCaptureObserver> screen_capture_observers_;
  base::ObserverList<ScreenShareObserver> screen_share_observers_;
  base::ObserverList<LastWindowClosedObserver> last_window_closed_observers_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
