// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/network_model.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

class InputEventsBlocker;
class NetworkView;
class ScreenManager;

namespace locale_util {
struct LanguageSwitchResult;
}

namespace login {
class NetworkStateHelper;
}

class NetworkScreen : public NetworkModel,
                      public NetworkStateHandlerObserver,
                      public input_method::InputMethodManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when enable debugging screen is requested.
    virtual void OnEnableDebuggingScreenRequested() = 0;
  };

  class Observer {
   public:
    virtual ~Observer() {}

    // Called when language list is reloaded.
    virtual void OnLanguageListReloaded() = 0;
  };

  NetworkScreen(BaseScreenDelegate* base_screen_delegate,
                Delegate* delegate,
                NetworkView* view);
  ~NetworkScreen() override;

  static NetworkScreen* Get(ScreenManager* manager);

  // NetworkModel implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Initialize(::login::ScreenContext* context) override;
  void OnViewDestroyed(NetworkView* view) override;
  void OnUserAction(const std::string& action_id) override;
  void OnContextKeyUpdated(const ::login::ScreenContext::KeyType& key) override;
  std::string GetLanguageListLocale() const override;
  const base::ListValue* GetLanguageList() const override;
  void UpdateLanguageList() override;

  // NetworkStateHandlerObserver implementation:
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* network) override;

  // InputMethodManager::Observer implementation:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          bool show_message) override;

  void SetApplicationLocale(const std::string& locale);
  std::string GetApplicationLocale();

  void SetInputMethod(const std::string& input_method);
  std::string GetInputMethod() const;

  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class NetworkScreenTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(NetworkScreenTest, CanConnect);

  // Subscribe to timezone changes.
  void InitializeTimezoneObserver();

  // Subscribes NetworkScreen to the network change notification,
  // forces refresh of current network state.
  void Refresh();

  // Sets the NetworkStateHelper for use in tests. This
  // class will take ownership of the pointed object.
  void SetNetworkStateHelperForTest(login::NetworkStateHelper* helper);

  // Subscribes to network change notifications.
  void SubscribeNetworkNotification();

  // Unsubscribes from network change notifications.
  void UnsubscribeNetworkNotification();

  // Notifies wizard on successful connection.
  void NotifyOnConnection();

  // Called by |connection_timer_| when connection to the network timed out.
  void OnConnectionTimeout();

  // Update UI based on current network status.
  void UpdateStatus();

  // Stops waiting for network to connect.
  void StopWaitingForConnection(const base::string16& network_id);

  // Starts waiting for network connection. Shows spinner.
  void WaitForConnection(const base::string16& network_id);

  // Called when continue button is pressed.
  void OnContinueButtonPressed();

  // Async callback after ReloadResourceBundle(locale) completed.
  void OnLanguageChangedCallback(
      const InputEventsBlocker* input_events_blocker,
      const locale_util::LanguageSwitchResult& result);

  // Starts resolving language list on BlockingPool.
  void ScheduleResolveLanguageList(
      scoped_ptr<locale_util::LanguageSwitchResult> language_switch_result);

  // Callback for chromeos::ResolveUILanguageList() (from l10n_util).
  void OnLanguageListResolved(scoped_ptr<base::ListValue> new_language_list,
                              std::string new_language_list_locale,
                              std::string new_selected_language);

  // Callback when the system timezone settings is changed.
  void OnSystemTimezoneChanged();

  // True if subscribed to network change notification.
  bool is_network_subscribed_;

  // ID of the the network that we are waiting for.
  base::string16 network_id_;

  // True if user pressed continue button so we should proceed with OOBE
  // as soon as we are connected.
  bool continue_pressed_;

  // Timer for connection timeout.
  base::OneShotTimer<NetworkScreen> connection_timer_;

  scoped_ptr<CrosSettings::ObserverSubscription> timezone_subscription_;

  NetworkView* view_;
  Delegate* delegate_;
  scoped_ptr<login::NetworkStateHelper> network_state_helper_;

  std::string input_method_;
  std::string timezone_;

  // Creation of language list happens on Blocking Pool, so we cache
  // resolved data.
  std::string language_list_locale_;
  scoped_ptr<base::ListValue> language_list_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  ObserverList<Observer> observers_;

  base::WeakPtrFactory<NetworkScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_SCREEN_H_
