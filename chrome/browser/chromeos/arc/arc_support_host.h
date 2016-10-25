// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"

// Native interface to control ARC support chrome App.
// TODO(hidehiko): Move more implementation for the UI control from
// ArcAuthService to this class.
// TODO(hidehiko,lhchavez,khmel): Extract preference observing into a
// standalone class so that it can be shared with OOBE flow.
// TODO(hidehiko,lhchavez): Move this into extensions/ directory, and put it
// into "arc" namespace. Add unittests at the time.
class ArcSupportHost : public arc::ArcSupportMessageHost::Observer,
                       public display::DisplayObserver {
 public:
  static const char kHostAppId[];
  static const char kStorageId[];

  ArcSupportHost();
  ~ArcSupportHost() override;

  // Called when the communication to arc_support Chrome App is ready.
  void SetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Called when the communication to arc_support Chrome App is closed.
  // The argument message_host is used to check if the given |message_host|
  // is what this instance uses know, to avoid racy case.
  // If |message_host| is different from the one this instance knows,
  // this is no op.
  void UnsetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Requests to close the extension window.
  void Close();
  void ShowPage(arc::ArcAuthService::UIPage page, const base::string16& status);

  // arc::ArcSupportMessageHost::Observer override:
  void OnMessage(const base::DictionaryValue& message) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  bool Initialize();
  void OnMetricsPreferenceChanged();
  void OnBackupAndRestorePreferenceChanged();
  void OnLocationServicePreferenceChanged();

  // Utilities on preference update.
  void SendMetricsMode();
  void SendBackupAndRestoreMode();
  void SendLocationServicesMode();
  void SendOptionMode(const std::string& action_name,
                      const std::string& pref_name);

  // Sends a preference update to the extension.
  // The message will be
  // {
  //   'action': action_name,
  //   'enabled': is_enabled,
  //   'managed': is_managed
  // }
  void SendPreferenceUpdate(const std::string& action_name,
                            bool is_enabled,
                            bool is_managed);

  void EnableMetrics(bool is_enabled);
  void EnableBackupRestore(bool is_enabled);
  void EnableLocationService(bool is_enabled);

  void DisconnectMessageHost();

  // The instance is created and managed by Chrome.
  arc::ArcSupportMessageHost* message_host_ = nullptr;

  // Used to track metrics preference.
  PrefChangeRegistrar pref_local_change_registrar_;
  // Used to track backup&restore and location service preference.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
