// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"

// Native interface to control ARC support chrome App.
// TODO(hidehiko): Move more implementation for the UI control from
// ArcAuthService to this class.
// TODO(hidehiko,lhchavez): Move this into extensions/ directory, and put it
// into "arc" namespace. Add unittests at the time.
class ArcSupportHost : public arc::ArcSupportMessageHost::Observer,
                       public display::DisplayObserver {
 public:
  enum class UIPage {
    NO_PAGE,              // Hide everything.
    TERMS,                // Terms content page.
    LSO_PROGRESS,         // LSO loading progress page.
    LSO,                  // LSO page to enter user's credentials.
    START_PROGRESS,       // Arc starting progress page.
    ERROR,                // Arc start error page.
    ERROR_WITH_FEEDBACK,  // Arc start error page, plus feedback button.
  };

  // Observer to notify UI event.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the ARC support window is closed.
    virtual void OnWindowClosed() = 0;

    // Called when the user press AGREE button on ToS page.
    // TODO(hidehiko): Currently, due to implementation reason,
    // this is also called when RETRY on error page is clicked. Fix this.
    virtual void OnTermsAgreed(bool is_metrics_enabled,
                               bool is_backup_and_restore_enabled,
                               bool is_location_service_enabled) = 0;

    // Called when LSO auth token fetch is successfully completed.
    virtual void OnAuthSucceeded(const std::string& auth_code) = 0;

    // Called when "RETRY" button on the error page is clicked.
    virtual void OnRetryClicked() = 0;

    // Called when send feedback button on error page is clicked.
    virtual void OnSendFeedbackClicked() = 0;
  };

  static const char kHostAppId[];
  static const char kStorageId[];

  ArcSupportHost();
  ~ArcSupportHost() override;

  // Currently only one observer can be added.
  // TODO(hidehiko): Support RemoveObserver. Support multiple observer.
  void AddObserver(Observer* observer);

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
  void ShowPage(UIPage page, const base::string16& status);

  void SetMetricsPreferenceCheckbox(bool is_enabled, bool is_managed);
  void SetBackupAndRestorePreferenceCheckbox(bool is_enabled, bool is_managed);
  void SetLocationServicesPreferenceCheckbox(bool is_enabled, bool is_managed);

  // arc::ArcSupportMessageHost::Observer override:
  void OnMessage(const base::DictionaryValue& message) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  struct PreferenceCheckboxData {
    PreferenceCheckboxData() : PreferenceCheckboxData(false, false) {}
    PreferenceCheckboxData(bool is_enabled, bool is_managed)
        : is_enabled(is_enabled), is_managed(is_managed) {}

    bool is_enabled;
    bool is_managed;
  };

  bool Initialize();

  // Sends a preference update to the extension.
  // The message will be
  // {
  //   'action': action_name,
  //   'enabled': is_enabled,
  //   'managed': is_managed
  // }
  void SendPreferenceCheckboxUpdate(const std::string& action_name,
                                    const PreferenceCheckboxData& data);

  void DisconnectMessageHost();

  // Currently Observer is only ArcAuthService, so it is unique.
  // Use ObserverList when more classes start to observe it.
  Observer* observer_ = nullptr;

  PreferenceCheckboxData metrics_checkbox_;
  PreferenceCheckboxData backup_and_restore_checkbox_;
  PreferenceCheckboxData location_services_checkbox_;

  // The instance is created and managed by Chrome.
  arc::ArcSupportMessageHost* message_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
