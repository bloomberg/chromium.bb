// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/arc/extensions/arc_support_message_host.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "ui/display/display_observer.h"

class Profile;

// Native interface to control ARC support chrome App.
// TODO(hidehiko,lhchavez): Move this into extensions/ directory, and put it
// into "arc" namespace. Add unittests at the time.
class ArcSupportHost : public arc::ArcSupportMessageHost::Observer,
                       public display::DisplayObserver {
 public:
  enum class UIPage {
    NO_PAGE,      // Hide everything.
    TERMS,        // Terms content page.
    LSO,          // LSO page to enter user's credentials.
    ARC_LOADING,  // ARC loading progress page.
    ERROR,        // ARC start error page.
  };

  // Error types whose corresponding message ARC support has.
  enum class Error {
    SIGN_IN_NETWORK_ERROR,
    SIGN_IN_SERVICE_UNAVAILABLE_ERROR,
    SIGN_IN_BAD_AUTHENTICATION_ERROR,
    SIGN_IN_GMS_NOT_AVAILABLE_ERROR,
    SIGN_IN_CLOUD_PROVISION_FLOW_FAIL_ERROR,
    SIGN_IN_UNKNOWN_ERROR,
    SERVER_COMMUNICATION_ERROR,
    ANDROID_MANAGEMENT_REQUIRED_ERROR,
  };

  // Observer to notify UI event.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the ARC support window is closed.
    virtual void OnWindowClosed() {}

    // Called when the user press AGREE button on ToS page.
    virtual void OnTermsAgreed(bool is_metrics_enabled,
                               bool is_backup_and_restore_enabled,
                               bool is_location_service_enabled) {}

    // Called when LSO auth token fetch is successfully completed.
    virtual void OnAuthSucceeded(const std::string& auth_code) {}

    // Called when "RETRY" button on the error page is clicked.
    virtual void OnRetryClicked() {}

    // Called when send feedback button on error page is clicked.
    virtual void OnSendFeedbackClicked() {}
  };

  static const char kHostAppId[];
  static const char kStorageId[];

  using RequestOpenAppCallback = base::Callback<void(Profile* profile)>;

  explicit ArcSupportHost(Profile* profile);
  ~ArcSupportHost() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  // Called when the communication to arc_support Chrome App is ready.
  void SetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Called when the communication to arc_support Chrome App is closed.
  // The argument message_host is used to check if the given |message_host|
  // is what this instance uses know, to avoid racy case.
  // If |message_host| is different from the one this instance knows,
  // this is no op.
  void UnsetMessageHost(arc::ArcSupportMessageHost* message_host);

  // Sets the ARC managed state. This must be called before ARC support app
  // is started.
  void SetArcManaged(bool is_arc_managed);

  // Requests to close the extension window.
  void Close();

  // Requests to show the "Terms Of Service" page.
  void ShowTermsOfService();

  // Requests to show the LSO page.
  void ShowLso();

  // Requests to show the "ARC is loading" page.
  void ShowArcLoading();

  // Requests to show the error page
  void ShowError(Error error, bool should_show_send_feedback);

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

  // Returns current page that has to be shown in OptIn UI.
  // Note that this can be inconsistent from the actually shown page.
  // TODO(hidehiko): Remove this exposure.
  UIPage ui_page() const { return ui_page_; }

  void SetRequestOpenAppCallbackForTesting(
      const RequestOpenAppCallback& callback);

 private:
  struct PreferenceCheckboxData {
    PreferenceCheckboxData() : PreferenceCheckboxData(false, false) {}
    PreferenceCheckboxData(bool is_enabled, bool is_managed)
        : is_enabled(is_enabled), is_managed(is_managed) {}

    bool is_enabled;
    bool is_managed;
  };

  // Requests to start the ARC support Chrome app.
  void RequestAppStart();

  bool Initialize();

  // Requests to ARC support Chrome app to show the specified page.
  void ShowPage(UIPage ui_page);

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

  Profile* const profile_;
  RequestOpenAppCallback request_open_app_callback_;

  base::ObserverList<Observer> observer_list_;

  // True, if ARC support app is requested to start, but the connection is not
  // yet established. Reset to false, when the app is started and the
  // connection to the app is established.
  bool app_start_pending_ = false;

  // The instance is created and managed by Chrome.
  arc::ArcSupportMessageHost* message_host_ = nullptr;

  // The lifetime of the message_host_ is out of control from ARC.
  // Fields below are UI parameter cache in case the value is set before
  // connection to the ARC support Chrome app is established.
  UIPage ui_page_ = UIPage::NO_PAGE;

  // These have valid values iff ui_page_ == ERROR.
  Error error_;
  bool should_show_send_feedback_;

  bool is_arc_managed_ = false;

  PreferenceCheckboxData metrics_checkbox_;
  PreferenceCheckboxData backup_and_restore_checkbox_;
  PreferenceCheckboxData location_services_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ArcSupportHost);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SUPPORT_HOST_H_
