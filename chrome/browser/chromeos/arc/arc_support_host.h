// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler_observer.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"

namespace arc {
class ArcOptInPreferenceHandler;
}

// Native interface to control ARC support chrome App.
// TODO(hidehiko): Move more implementation for the UI control from
// ArcAuthService to this class and remove
// arc::ArcOptInPreferenceHandlerObserver inheritance.
// TODO(hidehiko,lhchavez): Move this into extensions/ directory, and put it
// into "arc" namespace. Add unittests at the time.
class ArcSupportHost : public arc::ArcSupportMessageHost::Observer,
                       public arc::ArcOptInPreferenceHandlerObserver,
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

  // arc::ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

 private:
  bool Initialize();

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

  void DisconnectMessageHost();

  // The instance is created and managed by Chrome.
  arc::ArcSupportMessageHost* message_host_ = nullptr;

  // Handles preferences and metrics mode.
  std::unique_ptr<arc::ArcOptInPreferenceHandler> preference_handler_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
