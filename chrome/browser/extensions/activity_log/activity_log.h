// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;
using content::BrowserThread;

namespace extensions {
class Extension;
class ActivityLogPolicy;

// A utility for tracing interesting activity for each extension.
// It writes to an ActivityDatabase on a separate thread to record the activity.
class ActivityLog : public BrowserContextKeyedService,
                    public TabHelper::ScriptExecutionObserver,
                    public InstallObserver {
 public:
  // Observers can listen for activity events. There is probably only one
  // observer: the activityLogPrivate API.
  class Observer {
   public:
    virtual void OnExtensionActivity(scoped_refptr<Action> activity) = 0;
  };

  // ActivityLog is a singleton, so don't instantiate it with the constructor;
  // use GetInstance instead.
  static ActivityLog* GetInstance(Profile* profile);

  // Provides up-to-date information about whether the AL is enabled for a
  // profile. The AL is enabled if the user has installed the whitelisted
  // AL extension *or* set the --enable-extension-activity-logging flag.
  bool IsLogEnabled();

  // If you want to know whether the log is enabled but DON'T have a profile
  // object yet, use this method. However, it's preferable for the caller to
  // use IsLogEnabled when possible.
  static bool IsLogEnabledOnAnyProfile();

  // Add/remove observer: the activityLogPrivate API only listens when the
  // ActivityLog extension is registered for an event.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Log a successful API call made by an extension.
  // This will create an APIAction for storage in the database.
  // (Note: implemented as a wrapper for LogAPIActionInternal.)
  void LogAPIAction(const std::string& extension_id,
                    const std::string& name,    // e.g., tabs.get
                    base::ListValue* args,      // the argument values e.g. 46
                    const std::string& extra);  // any extra logging info

  // Log an event notification delivered to an extension.
  // This will create an APIAction for storage in the database.
  // (Note: implemented as a wrapper for LogAPIActionInternal.)
  void LogEventAction(const std::string& extension_id,
                      const std::string& name,    // e.g., tabs.onUpdate
                      base::ListValue* args,      // arguments to the callback
                      const std::string& extra);  // any extra logging info

  // Log a blocked API call made by an extension.
  // This will create a BlockedAction for storage in the database.
  void LogBlockedAction(const std::string& extension_id,
                        const std::string& blocked_call,  // e.g., tabs.get
                        base::ListValue* args,            // argument values
                        BlockedAction::Reason reason,     // why it's blocked
                        const std::string& extra);        // extra logging info

  // Log an interaction between an extension and a URL.
  // This will create a DOMAction for storage in the database.
  void LogDOMAction(const std::string& extension_id,
                    const GURL& url,                      // target URL
                    const string16& url_title,            // title of the URL
                    const std::string& api_call,          // api call
                    const base::ListValue* args,          // arguments
                    DomActionType::Type call_type,        // type of the call
                    const std::string& extra);            // extra logging info

  // Log a use of the WebRequest API to redirect, cancel, or modify page
  // headers.
  void LogWebRequestAction(const std::string& extension_id,
                           const GURL& url,
                           const std::string& api_call,
                           scoped_ptr<base::DictionaryValue> details,
                           const std::string& extra);

  // Retrieves the list of actions for a given extension on a specific day.
  // Today is 0, yesterday is 1, etc. Returns one day at a time.
  // Response is sent to the method/function in the callback.
  // Use base::Bind to create the callback.
  void GetActions(const std::string& extension_id,
                  const int day,
                  const base::Callback
                      <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>&
                      callback);

  // Extension::InstallObserver
  // We keep track of whether the whitelisted extension is installed; if it is,
  // we want to recompute whether to have logging enabled.
  virtual void OnExtensionInstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUninstalled(
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionDisabled(
      const extensions::Extension* extension) OVERRIDE;
  // We also have to list the following from InstallObserver.
  virtual void OnBeginExtensionInstall(const std::string& extension_id,
                                       const std::string& extension_name,
                                       const gfx::ImageSkia& installing_icon,
                                       bool is_app,
                                       bool is_platform_app) OVERRIDE {}
  virtual void OnDownloadProgress(const std::string& extension_id,
                                  int percent_downloaded) OVERRIDE {}
  virtual void OnInstallFailure(const std::string& extension_id) OVERRIDE {}
  virtual void OnAppsReordered() OVERRIDE {}
  virtual void OnAppInstalledToAppList(
      const std::string& extension_id) OVERRIDE {}
  virtual void OnShutdown() OVERRIDE {}

  // For unit tests only.
  // TODO(felt) In the future, when we'll have multiple policies, it might
  // be needed to rename the argument.
  void SetArgumentLoggingForTesting(bool log_arguments);
  static void RecomputeLoggingIsEnabled(bool profile_enabled);

  // BrowserContextKeyedService
  virtual void Shutdown() OVERRIDE;

  // At the moment, ActivityLog will use only one policy for summarization
  // (POLICY_NOARGS by default).  This static member function can be used
  // to change the default type, but has to be called before the first
  // GetInstance call.
  // TODO(dbabic,felt) ActivityLog should support multiple policies at the
  // same time, so this will need to be changed later.
  void SetDefaultPolicy(ActivityLogPolicy::PolicyType policy_type);

 private:
  friend class ActivityLogFactory;

  explicit ActivityLog(Profile* profile);
  virtual ~ActivityLog();

  // We log callbacks and API calls very similarly, so we handle them the same
  // way internally.
  void LogAPIActionInternal(
      const std::string& extension_id,
      const std::string& api_call,
      base::ListValue* args,
      const std::string& extra,
      const APIAction::Type type);

  // TabHelper::ScriptExecutionObserver implementation.
  // Fires when a ContentScript is executed.
  virtual void OnScriptsExecuted(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

  typedef ObserverListThreadSafe<Observer> ObserverList;
  scoped_refptr<ObserverList> observers_;

  // The policy object takes care of data summarization, compression, and
  // logging
  extensions::ActivityLogPolicy* policy_;

  // TODO(dbabic,felt) change this into a list of policy types later.
  ActivityLogPolicy::PolicyType policy_type_;

  Profile* profile_;
  // TODO(felt) These two flags could use a comment.
  bool enabled_;
  bool first_time_checking_;
  // testing_mode_ controls whether to log API call arguments. By default, we
  // don't log most arguments to avoid saving too much data. In testing mode,
  // argument collection is enabled. We also whitelist some arguments for
  // collection regardless of whether this bool is true.
  // When testing_mode_ is enabled, we also print to the console.
  bool testing_mode_;
  // We need the DB, FILE, and IO threads to operate. In some cases (tests),
  // these threads might not exist, so we avoid dispatching anything to the
  // ActivityDatabase to prevent things from exploding.
  bool has_threads_;

  InstallTracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLog);
};

// Each profile has different extensions, so we keep a different database for
// each profile.
class ActivityLogFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ActivityLog* GetForProfile(Profile* profile) {
    return static_cast<ActivityLog*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static ActivityLogFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ActivityLogFactory>;
  ActivityLogFactory();
  virtual ~ActivityLogFactory();

  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogFactory);
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_LOG_H_
