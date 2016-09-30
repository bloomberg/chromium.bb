// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"

// Supports communication with Arc support dialog.
class ArcSupportHost : public extensions::NativeMessageHost,
                       public arc::ArcAuthService::Observer,
                       public display::DisplayObserver {
 public:
  static const char kHostName[];
  static const char kHostAppId[];
  static const char kStorageId[];
  static const char* const kHostOrigin[];

  static std::unique_ptr<NativeMessageHost> Create();

  ~ArcSupportHost() override;

  // Requests to close the extension window.
  void Close();

  // Overrides NativeMessageHost:
  void Start(Client* client) override;
  void OnMessage(const std::string& request_string) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  // Overrides arc::ArcAuthService::Observer:
  // TODO(hidehiko): Get rid of Observer interface.
  void OnOptInUIClose() override;
  void OnOptInUIShowPage(arc::ArcAuthService::UIPage page,
                         const base::string16& status) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  ArcSupportHost();

  bool Initialize();
  void OnMetricsPreferenceChanged();
  void OnBackupAndRestorePreferenceChanged();
  void OnLocationServicePreferenceChanged();
  void SendMetricsMode();
  void SendBackupAndRestoreMode();
  void SendLocationServicesMode();
  void SendOptionMode(const std::string& action_name,
                      const std::string& pref_name);
  void EnableMetrics(bool is_enabled);
  void EnableBackupRestore(bool is_enabled);
  void EnableLocationService(bool is_enabled);

  // Unowned pointer.
  Client* client_ = nullptr;

  // Keep if Close() is requested from the browser.
  // TODO(hidehiko): Remove this. This is temporarily introduced for checking
  // if ArcAuthService::CancelAuthCode() needs to be invoked or not.
  // ArcAuthService should know its own state and the transition so moving to
  // there should simplify the structure. However, it is blocked by the current
  // dependency. For the clean up, more refactoring is needed, which can be
  // bigger changes.
  bool close_requested_ = false;

  // Used to track metrics preference.
  PrefChangeRegistrar pref_local_change_registrar_;
  // Used to track backup&restore and location service preference.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
