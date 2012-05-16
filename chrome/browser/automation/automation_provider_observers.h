// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/browsing_data_remover.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/options/take_photo_dialog.h"
#endif  // defined(OS_CHROMEOS)
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/password_manager/password_store_change.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "ui/gfx/size.h"

class AutomationProvider;
class BalloonCollection;
class Browser;
class Extension;
class ExtensionProcessManager;
class ExtensionService;
class InfoBarTabHelper;
class Notification;
class Profile;
class SavePackage;
class TranslateInfoBarDelegate;

#if defined(OS_CHROMEOS)
namespace chromeos {
  class ExistingUserController;
}
#endif  // defined(OS_CHROMEOS)

namespace IPC {
class Message;
}

namespace content {
class NavigationController;
class RenderViewHost;
class WebContents;
}

namespace history {
class TopSites;
}

namespace policy {
class BrowserPolicyConnector;
}

class InitialLoadObserver : public content::NotificationObserver {
 public:
  InitialLoadObserver(size_t tab_count, AutomationProvider* automation);
  virtual ~InitialLoadObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Caller owns the return value and is responsible for deleting it.
  // Example return value:
  // {'tabs': [{'start_time_ms': 1, 'stop_time_ms': 2.5},
  //           {'start_time_ms': 0.5, 'stop_time_ms': 3}]}
  // stop_time_ms values may be null if WaitForInitialLoads has not finished.
  // Only includes entries for the |tab_count| tabs we are monitoring.
  // There is no defined ordering of the return value.
  base::DictionaryValue* GetTimingInformation() const;

 private:
  class TabTime;
  typedef std::map<uintptr_t, TabTime> TabTimeMap;
  typedef std::set<uintptr_t> TabSet;

  void ConditionMet();

  content::NotificationRegistrar registrar_;

  base::WeakPtr<AutomationProvider> automation_;
  size_t crashed_tab_count_;
  size_t outstanding_tab_count_;
  base::TimeTicks init_time_;
  TabTimeMap loading_tabs_;
  TabSet finished_tabs_;

  DISALLOW_COPY_AND_ASSIGN(InitialLoadObserver);
};

#if defined(OS_CHROMEOS)
// Watches for NetworkManager events. Because NetworkLibrary loads
// asynchronously, this is used to make sure it is done before tests are run.
class NetworkManagerInitObserver
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  explicit NetworkManagerInitObserver(AutomationProvider* automation);
  virtual ~NetworkManagerInitObserver();
  virtual bool Init();
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);

 private:
  base::WeakPtr<AutomationProvider> automation_;

  DISALLOW_COPY_AND_ASSIGN(NetworkManagerInitObserver);
};

// Observes when the ChromeOS login WebUI becomes ready (by showing the login
// form, account picker, a network error or the OOBE wizard, depending on Chrome
// flags and state).
class LoginWebuiReadyObserver : public content::NotificationObserver {
 public:
  explicit LoginWebuiReadyObserver(AutomationProvider* automation);
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;

  DISALLOW_COPY_AND_ASSIGN(LoginWebuiReadyObserver);
};
#endif  // defined(OS_CHROMEOS)

// Watches for NewTabUI page loads for performance timing purposes.
class NewTabUILoadObserver : public content::NotificationObserver {
 public:
  explicit NewTabUILoadObserver(AutomationProvider* automation,
                                Profile* profile);
  virtual ~NewTabUILoadObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;

  DISALLOW_COPY_AND_ASSIGN(NewTabUILoadObserver);
};

class NavigationControllerRestoredObserver
    : public content::NotificationObserver {
 public:
  NavigationControllerRestoredObserver(
      AutomationProvider* automation,
      content::NavigationController* controller,
      IPC::Message* reply_message);
  virtual ~NavigationControllerRestoredObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  bool FinishedRestoring();
  void SendDone();

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  content::NavigationController* controller_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerRestoredObserver);
};

class NavigationNotificationObserver : public content::NotificationObserver {
 public:
  NavigationNotificationObserver(content::NavigationController* controller,
                                 AutomationProvider* automation,
                                 IPC::Message* reply_message,
                                 int number_of_navigations,
                                 bool include_current_navigation,
                                 bool use_json_interface);
  virtual ~NavigationNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  void ConditionMet(AutomationMsg_NavigationResponseValues navigation_result);

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  content::NavigationController* controller_;
  int navigations_remaining_;
  bool navigation_started_;
  bool use_json_interface_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class TabStripNotificationObserver : public content::NotificationObserver {
 public:
  TabStripNotificationObserver(int notification,
                               AutomationProvider* automation);
  virtual ~TabStripNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  virtual void ObserveTab(content::NavigationController* controller) = 0;

 protected:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  int notification_;
};

class TabAppendedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabAppendedNotificationObserver(Browser* parent,
                                  AutomationProvider* automation,
                                  IPC::Message* reply_message);
  virtual ~TabAppendedNotificationObserver();

  virtual void ObserveTab(content::NavigationController* controller);

 protected:
  Browser* parent_;
  scoped_ptr<IPC::Message> reply_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabAppendedNotificationObserver);
};

class TabClosedNotificationObserver : public TabStripNotificationObserver {
 public:
  TabClosedNotificationObserver(AutomationProvider* automation,
                                bool wait_until_closed,
                                IPC::Message* reply_message);
  virtual ~TabClosedNotificationObserver();

  virtual void ObserveTab(content::NavigationController* controller);

  void set_for_browser_command(bool for_browser_command);

 protected:
  scoped_ptr<IPC::Message> reply_message_;
  bool for_browser_command_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabClosedNotificationObserver);
};

// Notifies when the tab count reaches the target number.
class TabCountChangeObserver : public TabStripModelObserver {
 public:
  TabCountChangeObserver(AutomationProvider* automation,
                         Browser* browser,
                         IPC::Message* reply_message,
                         int target_tab_count);
  // Implementation of TabStripModelObserver.
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabStripModelDeleted();

 private:
  virtual ~TabCountChangeObserver();

  // Checks if the current tab count matches our target, and if so,
  // sends the reply message and deletes self.
  void CheckTabCount();

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  TabStripModel* tab_strip_model_;

  const int target_tab_count_;

  DISALLOW_COPY_AND_ASSIGN(TabCountChangeObserver);
};

// Observes when an extension has been uninstalled.
class ExtensionUninstallObserver : public content::NotificationObserver {
 public:
  ExtensionUninstallObserver(AutomationProvider* automation,
                             IPC::Message* reply_message,
                             const std::string& id);
  virtual ~ExtensionUninstallObserver();

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallObserver);
};

// Observes when an extension has finished loading and is ready for use. Also
// checks for possible install errors.
class ExtensionReadyNotificationObserver
    : public content::NotificationObserver {
 public:
  // Creates an observer that replies using the JSON automation interface.
  ExtensionReadyNotificationObserver(ExtensionProcessManager* manager,
                                     ExtensionService* service,
                                     AutomationProvider* automation,
                                     IPC::Message* reply_message);
  virtual ~ExtensionReadyNotificationObserver();

  // Implementation of NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  void Init();

  content::NotificationRegistrar registrar_;
  ExtensionProcessManager* manager_;
  ExtensionService* service_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionReadyNotificationObserver);
};

class ExtensionUnloadNotificationObserver
    : public content::NotificationObserver {
 public:
  ExtensionUnloadNotificationObserver();
  virtual ~ExtensionUnloadNotificationObserver();

  // Implementation of NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  bool did_receive_unload_notification() {
    return did_receive_unload_notification_;
  }

 private:
  content::NotificationRegistrar registrar_;
  bool did_receive_unload_notification_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUnloadNotificationObserver);
};

// Observes when the extensions have been fully updated.  The ExtensionUpdater
// service provides notifications for each extension that gets updated, but
// it does not wait for the updated extensions to be installed or loaded.  This
// observer waits until all updated extensions have actually been loaded.
class ExtensionsUpdatedObserver : public content::NotificationObserver {
 public:
  ExtensionsUpdatedObserver(ExtensionProcessManager* manager,
                            AutomationProvider* automation,
                            IPC::Message* reply_message);
  virtual ~ExtensionsUpdatedObserver();

  // Implementation of NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  ExtensionProcessManager* manager_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  std::set<std::string> in_progress_updates_;
  bool updater_finished_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsUpdatedObserver);
};

// Observes when a new browser has been opened and a tab within it has stopped
// loading.
class BrowserOpenedNotificationObserver : public content::NotificationObserver {
 public:
  BrowserOpenedNotificationObserver(AutomationProvider* automation,
                                    IPC::Message* reply_message);
  virtual ~BrowserOpenedNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  void set_for_browser_command(bool for_browser_command);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  int new_window_id_;
  bool for_browser_command_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOpenedNotificationObserver);
};

class BrowserClosedNotificationObserver : public content::NotificationObserver {
 public:
  BrowserClosedNotificationObserver(Browser* browser,
                                    AutomationProvider* automation,
                                    IPC::Message* reply_message);
  virtual ~BrowserClosedNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  void set_for_browser_command(bool for_browser_command);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  bool for_browser_command_;

  DISALLOW_COPY_AND_ASSIGN(BrowserClosedNotificationObserver);
};

class BrowserCountChangeNotificationObserver
    : public content::NotificationObserver {
 public:
  BrowserCountChangeNotificationObserver(int target_count,
                                         AutomationProvider* automation,
                                         IPC::Message* reply_message);
  virtual ~BrowserCountChangeNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  int target_count_;
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCountChangeNotificationObserver);
};

class AppModalDialogShownObserver : public content::NotificationObserver {
 public:
  AppModalDialogShownObserver(AutomationProvider* automation,
                              IPC::Message* reply_message);
  virtual ~AppModalDialogShownObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogShownObserver);
};

class ExecuteBrowserCommandObserver : public content::NotificationObserver {
 public:
  virtual ~ExecuteBrowserCommandObserver();

  static bool CreateAndRegisterObserver(AutomationProvider* automation,
                                        Browser* browser,
                                        int command,
                                        IPC::Message* reply_message);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  ExecuteBrowserCommandObserver(AutomationProvider* automation,
                                IPC::Message* reply_message);

  bool Register(int command);

  bool Getint(int command, int* type);

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  int notification_type_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(ExecuteBrowserCommandObserver);
};

class FindInPageNotificationObserver : public content::NotificationObserver {
 public:
  FindInPageNotificationObserver(AutomationProvider* automation,
                                 content::WebContents* parent_tab,
                                 bool reply_with_json,
                                 IPC::Message* reply_message);
  virtual ~FindInPageNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // The Find mechanism is over asynchronous IPC, so a search is kicked off and
  // we wait for notification to find out what the results are. As the user is
  // typing, new search requests can be issued and the Request ID helps us make
  // sense of whether this is the current request or an old one. The unit tests,
  // however, which uses this constant issues only one search at a time, so we
  // don't need a rolling id to identify each search. But, we still need to
  // specify one, so we just use a fixed one - its value does not matter.
  static const int kFindInPageRequestId;

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  // Send reply using json automation interface.
  bool reply_with_json_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(FindInPageNotificationObserver);
};

class DomOperationObserver : public content::NotificationObserver {
 public:
  explicit DomOperationObserver(int automation_id);
  virtual ~DomOperationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  virtual void OnDomOperationCompleted(const std::string& json) = 0;
  virtual void OnModalDialogShown() = 0;
  virtual void OnJavascriptBlocked() = 0;

 private:
  int automation_id_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DomOperationObserver);
};

// Sends a message back to the automation client with the results of the DOM
// operation.
class DomOperationMessageSender : public DomOperationObserver {
 public:
  DomOperationMessageSender(AutomationProvider* automation,
                            IPC::Message* reply_message,
                            bool use_json_interface);
  virtual ~DomOperationMessageSender();

  virtual void OnDomOperationCompleted(const std::string& json) OVERRIDE;
  virtual void OnModalDialogShown() OVERRIDE;
  virtual void OnJavascriptBlocked() OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  bool use_json_interface_;

  DISALLOW_COPY_AND_ASSIGN(DomOperationMessageSender);
};

// Collects METRIC_EVENT_DURATION notifications and keep track of the times.
class MetricEventDurationObserver : public content::NotificationObserver {
 public:
  MetricEventDurationObserver();
  virtual ~MetricEventDurationObserver();

  // Get the duration of an event.  Returns -1 if we haven't seen the event.
  int GetEventDurationMs(const std::string& event_name);

  // NotificationObserver interface.
  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;

  typedef std::map<std::string, int> EventDurationMap;
  EventDurationMap durations_;

  DISALLOW_COPY_AND_ASSIGN(MetricEventDurationObserver);
};

class PageTranslatedObserver : public content::NotificationObserver {
 public:
  PageTranslatedObserver(AutomationProvider* automation,
                         IPC::Message* reply_message,
                         content::WebContents* web_contents);
  virtual ~PageTranslatedObserver();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(PageTranslatedObserver);
};

class TabLanguageDeterminedObserver : public content::NotificationObserver {
 public:
  TabLanguageDeterminedObserver(AutomationProvider* automation,
                                IPC::Message* reply_message,
                                content::WebContents* web_contents,
                                TranslateInfoBarDelegate* translate_bar);
  virtual ~TabLanguageDeterminedObserver();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  content::WebContents* web_contents_;
  TranslateInfoBarDelegate* translate_bar_;

  DISALLOW_COPY_AND_ASSIGN(TabLanguageDeterminedObserver);
};

class InfoBarCountObserver : public content::NotificationObserver {
 public:
  InfoBarCountObserver(AutomationProvider* automation,
                       IPC::Message* reply_message,
                       TabContentsWrapper* tab_contents,
                       size_t target_count);
  virtual ~InfoBarCountObserver();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  // Checks whether the infobar count matches our target, and if so
  // sends the reply message and deletes itself.
  void CheckCount();

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  TabContentsWrapper* tab_contents_;

  const size_t target_count_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarCountObserver);
};

#if defined(OS_CHROMEOS)
class LoginObserver : public chromeos::LoginStatusConsumer {
 public:
  LoginObserver(chromeos::ExistingUserController* controller,
                AutomationProvider* automation,
                IPC::Message* reply_message);

  virtual ~LoginObserver();

  virtual void OnLoginFailure(const chromeos::LoginFailure& error);

  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      bool pending_requests,
      bool using_oauth);

 private:
  chromeos::ExistingUserController* controller_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(LoginObserver);
};

// Collects SCREEN_LOCK_STATE_CHANGED notifications and returns
// whether authentication succeeded to the automation provider.
class ScreenLockUnlockObserver : public content::NotificationObserver {
 public:
  // Set lock_screen to true to observe lock screen events,
  // false for unlock screen events.
  ScreenLockUnlockObserver(AutomationProvider* automation,
                           IPC::Message* reply_message,
                           bool lock_screen);
  virtual ~ScreenLockUnlockObserver();

  // content::NotificationObserver interface.
  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 protected:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

 private:
  content::NotificationRegistrar registrar_;
  bool lock_screen_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockUnlockObserver);
};

// Watches SCREEN_LOCK_STATE_CHANGED notifications like the
// ScreenLockUnlockObserver, but additionally adds itself as an observer
// to a screen locker in order to monitor unlock failure cases.
class ScreenUnlockObserver : public ScreenLockUnlockObserver,
                             public chromeos::LoginStatusConsumer {
 public:
  ScreenUnlockObserver(AutomationProvider* automation,
                       IPC::Message* reply_message);
  virtual ~ScreenUnlockObserver();

  virtual void OnLoginFailure(const chromeos::LoginFailure& error);

  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      bool pending_requests,
      bool using_oauth) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenUnlockObserver);
};

class NetworkScanObserver
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  NetworkScanObserver(AutomationProvider* automation,
                      IPC::Message* reply_message);

  virtual ~NetworkScanObserver();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScanObserver);
};

class ToggleNetworkDeviceObserver
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  ToggleNetworkDeviceObserver(AutomationProvider* automation,
                              IPC::Message* reply_message,
                              const std::string& device,
                              bool enable);

  virtual ~ToggleNetworkDeviceObserver();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  std::string device_;
  bool enable_;

  DISALLOW_COPY_AND_ASSIGN(ToggleNetworkDeviceObserver);
};

class NetworkStatusObserver
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  NetworkStatusObserver(AutomationProvider* automation,
                        IPC::Message* reply_message);
  virtual ~NetworkStatusObserver();

  virtual const chromeos::Network* GetNetwork(
      chromeos::NetworkLibrary* network_library) = 0;
  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);
  virtual void NetworkStatusCheck(const chromeos::Network* network) = 0;

 protected:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStatusObserver);
};

// Waits for a connection success or failure for the specified
// network and returns the status to the automation provider.
class NetworkConnectObserver : public NetworkStatusObserver {
 public:
  NetworkConnectObserver(AutomationProvider* automation,
                         IPC::Message* reply_message);

  virtual void NetworkStatusCheck(const chromeos::Network* network);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectObserver);
};

// Waits until a network has disconnected.  Then returns success
// or failure.
class NetworkDisconnectObserver : public NetworkStatusObserver {
 public:
  NetworkDisconnectObserver(AutomationProvider* automation,
                            IPC::Message* reply_message,
                            const std::string& service_path);

  virtual void NetworkStatusCheck(const chromeos::Network* network);
  const chromeos::Network* GetNetwork(
      chromeos::NetworkLibrary* network_library);

 private:
  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(NetworkDisconnectObserver);
};

// Waits for a connection success or failure for the specified
// network and returns the status to the automation provider.
class ServicePathConnectObserver : public NetworkConnectObserver {
 public:
  ServicePathConnectObserver(AutomationProvider* automation,
                             IPC::Message* reply_message,
                             const std::string& service_path);

  const chromeos::Network* GetNetwork(
      chromeos::NetworkLibrary* network_library);

 private:
  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(ServicePathConnectObserver);
};

// Waits for a connection success or failure for the specified
// network and returns the status to the automation provider.
class SSIDConnectObserver : public NetworkConnectObserver {
 public:
  SSIDConnectObserver(AutomationProvider* automation,
                      IPC::Message* reply_message,
                      const std::string& ssid);

  const chromeos::Network* GetNetwork(
      chromeos::NetworkLibrary* network_library);

 private:
  std::string ssid_;
  DISALLOW_COPY_AND_ASSIGN(SSIDConnectObserver);
};

// Waits for a connection success or failure for the specified
// virtual network and returns the status to the automation provider.
class VirtualConnectObserver
    : public chromeos::NetworkLibrary::NetworkManagerObserver {
 public:
  VirtualConnectObserver(AutomationProvider* automation,
                         IPC::Message* reply_message,
                         const std::string& service_name);

  virtual ~VirtualConnectObserver();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros);

 private:
  virtual chromeos::VirtualNetwork* GetVirtualNetwork(
      const chromeos::NetworkLibrary* cros);

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  std::string service_name_;

  DISALLOW_COPY_AND_ASSIGN(VirtualConnectObserver);
};

// Waits for enterprise device enrollment to complete and returns the status to
// the automation provider.
class EnrollmentObserver
    : public chromeos::EnterpriseEnrollmentScreenActor::Observer {
 public:
  EnrollmentObserver(AutomationProvider* automation,
      IPC::Message* reply_message,
      chromeos::EnterpriseEnrollmentScreenActor* enrollment_screen_actor,
      chromeos::EnterpriseEnrollmentScreen* enrollment_screen);

  virtual ~EnrollmentObserver();

  // chromeos::EnterpriseEnrollmentScreenActor::Observer implementation.
  virtual void OnEnrollmentComplete(
      chromeos::EnterpriseEnrollmentScreenActor* enrollment_screen_actor,
      bool succeeded);

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  chromeos::EnterpriseEnrollmentScreen* enrollment_screen_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentObserver);
};

// Waits for profile photo to be captured by the camera,
// saved to file, and the path set in local state preferences
class PhotoCaptureObserver : public chromeos::TakePhotoDialog::Observer,
                             public chromeos::UserManager::Observer {
 public:
  PhotoCaptureObserver(AutomationProvider* automation,
                       IPC::Message* reply_message);
  virtual ~PhotoCaptureObserver();

  // TakePhotoDialog::Observer overrides
  virtual void OnCaptureSuccess(
      chromeos::TakePhotoDialog* dialog,
      chromeos::TakePhotoView* take_photo_view) OVERRIDE;
  virtual void OnCaptureFailure(
      chromeos::TakePhotoDialog* dialog,
      chromeos::TakePhotoView* take_photo_view) OVERRIDE;
  virtual void OnCapturingStopped(
      chromeos::TakePhotoDialog* dialog,
      chromeos::TakePhotoView* take_photo_view) OVERRIDE;

  // UserManager::Observer overrides
  virtual void LocalStateChanged(
      chromeos::UserManager* user_manager) OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(PhotoCaptureObserver);
};

#endif  // defined(OS_CHROMEOS)

// Waits for the bookmark model to load.
class AutomationProviderBookmarkModelObserver : public BookmarkModelObserver {
 public:
  AutomationProviderBookmarkModelObserver(AutomationProvider* provider,
                                          IPC::Message* reply_message,
                                          BookmarkModel* model);
  virtual ~AutomationProviderBookmarkModelObserver();

  // BookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE {}
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE {}
  virtual void BookmarkNodeChildrenReordered(
      BookmarkModel* model,
      const BookmarkNode* node) OVERRIDE {}

 private:
  // Reply to the automation message with the given success value,
  // then delete myself (which removes myself from the bookmark model
  // observer list).
  void ReplyAndDelete(bool success);

  base::WeakPtr<AutomationProvider> automation_provider_;
  scoped_ptr<IPC::Message> reply_message_;
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProviderBookmarkModelObserver);
};

// Allows the automation provider to wait until the download has been updated
// or opened.
class AutomationProviderDownloadUpdatedObserver
    : public content::DownloadItem::Observer {
 public:
  AutomationProviderDownloadUpdatedObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message,
      bool wait_for_open);
  virtual ~AutomationProviderDownloadUpdatedObserver();

  virtual void OnDownloadUpdated(content::DownloadItem* download);
  virtual void OnDownloadOpened(content::DownloadItem* download);

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
  bool wait_for_open_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProviderDownloadUpdatedObserver);
};

// Allows the automation provider to wait until the download model has changed
// (because a new download has been added or removed).
class AutomationProviderDownloadModelChangedObserver
    : public content::DownloadManager::Observer {
 public:
  AutomationProviderDownloadModelChangedObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message,
      content::DownloadManager* download_manager);
  virtual ~AutomationProviderDownloadModelChangedObserver();

  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
  content::DownloadManager* download_manager_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProviderDownloadModelChangedObserver);
};

// Observes when all pending downloads have completed.
class AllDownloadsCompleteObserver
    : public content::DownloadManager::Observer,
      public content::DownloadItem::Observer {
 public:
  AllDownloadsCompleteObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message,
      content::DownloadManager* download_manager,
      ListValue* pre_download_ids);
  virtual ~AllDownloadsCompleteObserver();

  // content::DownloadManager::Observer.
  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;

  // content::DownloadItem::Observer.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {}

 private:
  void ReplyIfNecessary();

  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
  content::DownloadManager* download_manager_;
  std::set<int> pre_download_ids_;
  std::set<content::DownloadItem*> pending_downloads_;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadsCompleteObserver);
};

// Allows automation provider to wait until TemplateURLService has loaded
// before looking up/returning search engine info.
class AutomationProviderSearchEngineObserver
    : public TemplateURLServiceObserver {
 public:
  AutomationProviderSearchEngineObserver(
      AutomationProvider* provider,
      Profile* profile,
      IPC::Message* reply_message);
  virtual ~AutomationProviderSearchEngineObserver();

  virtual void OnTemplateURLServiceChanged();

 private:
  base::WeakPtr<AutomationProvider> provider_;
  Profile* profile_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(AutomationProviderSearchEngineObserver);
};

// Allows the automation provider to wait for history queries to finish.
class AutomationProviderHistoryObserver {
 public:
  AutomationProviderHistoryObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message);
  virtual ~AutomationProviderHistoryObserver();

  void HistoryQueryComplete(HistoryService::Handle request_handle,
                            history::QueryResults* results);

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
};

// Allows the automation provider to wait for import queries to finish.
class AutomationProviderImportSettingsObserver
    : public importer::ImporterProgressObserver {
 public:
  AutomationProviderImportSettingsObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message);
  virtual ~AutomationProviderImportSettingsObserver();

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
};

// Allows automation provider to wait for getting passwords to finish.
class AutomationProviderGetPasswordsObserver : public PasswordStoreConsumer {
 public:
  AutomationProviderGetPasswordsObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message);
  virtual ~AutomationProviderGetPasswordsObserver();

  virtual void OnPasswordStoreRequestDone(
      CancelableRequestProvider::Handle handle,
      const std::vector<webkit::forms::PasswordForm*>& result) OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
};

// Observes when login entries stored in the password store are changed.  The
// notifications are sent on the DB thread, the thread that interacts with the
// web database.
class PasswordStoreLoginsChangedObserver
    : public base::RefCountedThreadSafe<
          PasswordStoreLoginsChangedObserver,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  PasswordStoreLoginsChangedObserver(AutomationProvider* automation,
                                     IPC::Message* reply_message,
                                     PasswordStoreChange::Type expected_type,
                                     const std::string& result_key);

  // Schedules a task on the DB thread to register the appropriate observers.
  virtual void Init();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  ~PasswordStoreLoginsChangedObserver();
  friend class base::DeleteHelper<PasswordStoreLoginsChangedObserver>;

  // Registers the appropriate observers.  Called on the DB thread.
  void RegisterObserversTask();

  // Sends the |reply_message_| to |automation_| indicating we're done.  Called
  // on the UI thread.
  void IndicateDone();

  // Sends an error reply to |automation_|.  Called on the UI thread.
  void IndicateError(const std::string& error);

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  scoped_ptr<content::NotificationRegistrar> registrar_;
  PasswordStoreChange::Type expected_type_;
  std::string result_key_;

  // Used to ensure that the UI thread waits for the DB thread to finish
  // registering observers before proceeding.
  base::WaitableEvent done_event_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreLoginsChangedObserver);
};

// Allows the automation provider to wait for clearing browser data to finish.
class AutomationProviderBrowsingDataObserver
    : public BrowsingDataRemover::Observer {
 public:
  AutomationProviderBrowsingDataObserver(
      AutomationProvider* provider,
      IPC::Message* reply_message);
  virtual ~AutomationProviderBrowsingDataObserver();

  virtual void OnBrowsingDataRemoverDone();

 private:
  base::WeakPtr<AutomationProvider> provider_;
  scoped_ptr<IPC::Message> reply_message_;
};

// Allows automation provider to wait until page load after selecting an item
// in the omnibox popup.
class OmniboxAcceptNotificationObserver : public content::NotificationObserver {
 public:
   OmniboxAcceptNotificationObserver(content::NavigationController* controller,
                                     AutomationProvider* automation,
                                     IPC::Message* reply_message);
  virtual ~OmniboxAcceptNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  content::NavigationController* controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxAcceptNotificationObserver);
};

// Allows the automation provider to wait for a save package notification.
class SavePackageNotificationObserver : public content::NotificationObserver {
 public:
  SavePackageNotificationObserver(content::DownloadManager* download_manager,
                                  AutomationProvider* automation,
                                  IPC::Message* reply_message);
  virtual ~SavePackageNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageNotificationObserver);
};

// This class manages taking a snapshot of a page.
class PageSnapshotTaker : public TabEventObserver,
                          public content::NotificationObserver {
 public:
  PageSnapshotTaker(AutomationProvider* automation,
                    IPC::Message* reply_message,
                    TabContentsWrapper* tab_contents,
                    const FilePath& path);
  virtual ~PageSnapshotTaker();

  // Start the process of taking a snapshot of the entire page.
  void Start();

 private:
  // TabEventObserver overrides.
  virtual void OnSnapshotEntirePageACK(
      bool success,
      const std::vector<unsigned char>& png_data,
      const std::string& error_msg) OVERRIDE;
  // NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Helper method to send a response back to the client. Deletes this.
  void SendMessage(bool success, const std::string& error_msg);

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  TabContentsWrapper* tab_contents_;
  FilePath image_path_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PageSnapshotTaker);
};

class NTPInfoObserver : public content::NotificationObserver {
 public:
  NTPInfoObserver(AutomationProvider* automation,
                  IPC::Message* reply_message,
                  CancelableRequestConsumer* consumer);
  virtual ~NTPInfoObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  void OnTopSitesLoaded();
  void OnTopSitesReceived(const history::MostVisitedURLList& visited_list);

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  CancelableRequestConsumer* consumer_;
  CancelableRequestProvider::Handle request_;
  scoped_ptr<base::DictionaryValue> ntp_info_;
  history::TopSites* top_sites_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(NTPInfoObserver);
};

// Observes when an app has been launched, as indicated by a notification that
// a content load in some tab has stopped.
class AppLaunchObserver : public content::NotificationObserver {
 public:
  AppLaunchObserver(content::NavigationController* controller,
                    AutomationProvider* automation,
                    IPC::Message* reply_message,
                    extension_misc::LaunchContainer launch_container);
  virtual ~AppLaunchObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NavigationController* controller_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  content::NotificationRegistrar registrar_;
  extension_misc::LaunchContainer launch_container_;
  int new_window_id_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchObserver);
};

// Observes when Autofill information is displayed in the renderer.  This can
// happen in two different ways: (1) a popup containing Autofill suggestions
// has been shown in the renderer; (2) a webpage form is filled or previewed
// with Autofill suggestions.  A constructor argument specifies the appropriate
// notification to wait for.
class AutofillDisplayedObserver : public content::NotificationObserver {
 public:
  AutofillDisplayedObserver(int notification,
                            content::RenderViewHost* render_view_host,
                            AutomationProvider* automation,
                            IPC::Message* reply_message);
  virtual ~AutofillDisplayedObserver();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  int notification_;
  content::RenderViewHost* render_view_host_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDisplayedObserver);
};

// Observes when a specified number of autofill profiles and credit cards have
// been changed in the WebDataService.  The notifications are sent on the DB
// thread, the thread that interacts with the database.
class AutofillChangedObserver
    : public base::RefCountedThreadSafe<
          AutofillChangedObserver,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  AutofillChangedObserver(AutomationProvider* automation,
                          IPC::Message* reply_message,
                          int num_profiles,
                          int num_credit_cards);

  // Schedules a task on the DB thread to register the appropriate observers.
  virtual void Init();

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  ~AutofillChangedObserver();
  friend class base::DeleteHelper<AutofillChangedObserver>;

  // Registers the appropriate observers.  Called on the DB thread.
  void RegisterObserversTask();

  // Sends the |reply_message_| to |automation_| indicating we're done.  Called
  // on the UI thread.
  void IndicateDone();

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  scoped_ptr<content::NotificationRegistrar> registrar_;
  int num_profiles_;
  int num_credit_cards_;

  // Used to ensure that the UI thread waits for the DB thread to finish
  // registering observers before proceeding.
  base::WaitableEvent done_event_;

  DISALLOW_COPY_AND_ASSIGN(AutofillChangedObserver);
};

// Observes when an Autofill form submitted via a webpage has been processed.
// This observer also takes care of accepting any infobars that appear as a
// result of submitting the webpage form (submitting credit card information
// causes a confirm infobar to appear).
class AutofillFormSubmittedObserver
    : public PersonalDataManagerObserver,
      public content::NotificationObserver {
 public:
  AutofillFormSubmittedObserver(AutomationProvider* automation,
                                IPC::Message* reply_message,
                                PersonalDataManager* pdm);
  virtual ~AutofillFormSubmittedObserver();

  // PersonalDataManagerObserver interface.
  virtual void OnPersonalDataChanged() OVERRIDE;
  virtual void OnInsufficientFormData() OVERRIDE;

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  PersonalDataManager* pdm_;
  InfoBarTabHelper* infobar_helper_;
};

// Allows the automation provider to wait until all the notification
// processes are ready.
class GetAllNotificationsObserver : public content::NotificationObserver {
 public:
  GetAllNotificationsObserver(AutomationProvider* automation,
                              IPC::Message* reply_message);
  virtual ~GetAllNotificationsObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  // Sends a message via the |AutomationProvider|. |automation_| must be valid.
  // Deletes itself after the message is sent.
  void SendMessage();
  // Returns a new dictionary describing the given notification.
  base::DictionaryValue* NotificationToJson(const Notification* note);

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(GetAllNotificationsObserver);
};

// Allows the automation provider to wait for a new notification balloon
// to appear and be ready.
class NewNotificationBalloonObserver : public content::NotificationObserver {
 public:
  NewNotificationBalloonObserver(AutomationProvider* provider,
                                 IPC::Message* reply_message);
  virtual ~NewNotificationBalloonObserver();
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NewNotificationBalloonObserver);
};

// Allows the automation provider to wait for a given number of
// notification balloons.
class OnNotificationBalloonCountObserver
    : public content::NotificationObserver {
 public:
  OnNotificationBalloonCountObserver(AutomationProvider* provider,
                                     IPC::Message* reply_message,
                                     int count);
  virtual ~OnNotificationBalloonCountObserver();

  // Sends an automation reply message if |automation_| is still valid and the
  // number of ready balloons matches the desired count. Deletes itself if the
  // message is sent or if |automation_| is invalid.
  void CheckBalloonCount();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  BalloonCollection* collection_;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(OnNotificationBalloonCountObserver);
};

// Allows the automation provider to wait for a RENDERER_PROCESS_CLOSED
// notification.
class RendererProcessClosedObserver : public content::NotificationObserver {
 public:
  RendererProcessClosedObserver(AutomationProvider* automation,
                                IPC::Message* reply_message);
  virtual ~RendererProcessClosedObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(RendererProcessClosedObserver);
};

// Allows the automation provider to wait for acknowledgement that a certain
// type and number of input events has been processed by the renderer.
class InputEventAckNotificationObserver : public content::NotificationObserver {
 public:
  InputEventAckNotificationObserver(AutomationProvider* automation,
                                    IPC::Message* reply_message,
                                    int event_type, int count);
  virtual ~InputEventAckNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  int event_type_;
  int count_;

  DISALLOW_COPY_AND_ASSIGN(InputEventAckNotificationObserver);
};

// Allows the automation provider to wait for all render views to finish any
// pending loads. This wait is only guaranteed for views that exist at the
// observer's creation. Will send a message on construction if no views are
// currently loading.
class AllViewsStoppedLoadingObserver : public TabEventObserver,
                                       public content::NotificationObserver {
 public:
  AllViewsStoppedLoadingObserver(
      AutomationProvider* automation,
      IPC::Message* reply_message,
      ExtensionProcessManager* extension_process_manager);
  virtual ~AllViewsStoppedLoadingObserver();

  // TabEventObserver implementation.
  virtual void OnFirstPendingLoad(content::WebContents* web_contents) OVERRIDE;
  virtual void OnNoMorePendingLoads(
      content::WebContents* web_contents) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  typedef std::set<content::WebContents*> TabSet;

  // Checks if there are no pending loads. If none, it will send an automation
  // relpy and delete itself.
  void CheckIfNoMorePendingLoads();

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  ExtensionProcessManager* extension_process_manager_;
  content::NotificationRegistrar registrar_;
  TabSet pending_tabs_;

  DISALLOW_COPY_AND_ASSIGN(AllViewsStoppedLoadingObserver);
};

// Observer used to listen for new tab creation to complete.
class NewTabObserver : public content::NotificationObserver {
 public:
  NewTabObserver(AutomationProvider* automation, IPC::Message* reply_message);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  virtual ~NewTabObserver();

  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(NewTabObserver);
};

// Posts a task to the PROCESS_LAUNCHER thread, once processed posts a task
// back to the UI thread that notifies the provider we're done.
class WaitForProcessLauncherThreadToGoIdleObserver
    : public base::RefCountedThreadSafe<
          WaitForProcessLauncherThreadToGoIdleObserver,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  WaitForProcessLauncherThreadToGoIdleObserver(
      AutomationProvider* automation, IPC::Message* reply_message);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WaitForProcessLauncherThreadToGoIdleObserver>;

  virtual ~WaitForProcessLauncherThreadToGoIdleObserver();

  // Schedules a task on the PROCESS_LAUNCHER thread to execute
  // |RunOnProcessLauncherThread2|. By the time the task is executed the
  // PROCESS_LAUNCHER thread should be some what idle.
  void RunOnProcessLauncherThread();

  // When executed the PROCESS_LAUNCHER thread should have processed any pending
  // tasks.  Schedules a task on the UI thread that sends the message saying
  // we're done.
  void RunOnProcessLauncherThread2();

  // Sends the |reply_message_| to |automation_| indicating we're done.
  void RunOnUIThread();

  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(WaitForProcessLauncherThreadToGoIdleObserver);
};

// Allows the automation provider to wait for acknowledgement that a drop
// operation has been processed by the renderer.
class DragTargetDropAckNotificationObserver
    : public content::NotificationObserver {
 public:
  DragTargetDropAckNotificationObserver(AutomationProvider* automation,
                                        IPC::Message* reply_message);
  virtual ~DragTargetDropAckNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(DragTargetDropAckNotificationObserver);
};

// Allows the automation provider to wait for process memory details to be
// available before sending this information to the client.
class ProcessInfoObserver : public MemoryDetails {
 public:
  ProcessInfoObserver(AutomationProvider* automation,
                      IPC::Message* reply_message);

  virtual void OnDetailsAvailable() OVERRIDE;

 private:
  virtual ~ProcessInfoObserver();
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInfoObserver);
};

// Observes when new v8 heap statistics are computed for a renderer process.
class V8HeapStatsObserver : public content::NotificationObserver {
 public:
  V8HeapStatsObserver(AutomationProvider* automation,
                      IPC::Message* reply_message,
                      base::ProcessId renderer_id);
  virtual ~V8HeapStatsObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  base::ProcessId renderer_id_;

  DISALLOW_COPY_AND_ASSIGN(V8HeapStatsObserver);
};

// Observes when a new FPS value is computed for a renderer process.
class FPSObserver : public content::NotificationObserver {
 public:
  FPSObserver(AutomationProvider* automation,
              IPC::Message* reply_message,
              base::ProcessId renderer_id,
              int routing_id);
  virtual ~FPSObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  base::ProcessId renderer_id_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(FPSObserver);
};

// Manages the process of creating a new Profile and opening a new browser with
// that profile. This observer should be created, and then a new Profile
// should be created through the ProfileManager using
// profile_manager->CreateMultiProfileAsync(). The Observer watches for profile
// creation, upon which it creates a new browser window; after observing
// window creation, it creates a new tab and then finally observes it finish
// loading.
class BrowserOpenedWithNewProfileNotificationObserver
    : public content::NotificationObserver {
 public:
  BrowserOpenedWithNewProfileNotificationObserver(
      AutomationProvider* automation,
      IPC::Message* reply_message);
  virtual ~BrowserOpenedWithNewProfileNotificationObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  content::NotificationRegistrar registrar_;
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  int new_window_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserOpenedWithNewProfileNotificationObserver);
};

// Waits for an extension popup to appear and load.
class ExtensionPopupObserver : public content::NotificationObserver {
 public:
  ExtensionPopupObserver(
      AutomationProvider* automation,
      IPC::Message* reply_message,
      const std::string& extension_id);
  ~ExtensionPopupObserver();

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  base::WeakPtr<AutomationProvider> automation_;
  scoped_ptr<IPC::Message> reply_message_;
  std::string extension_id_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopupObserver);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_PROVIDER_OBSERVERS_H_
