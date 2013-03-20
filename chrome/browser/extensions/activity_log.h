// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chrome/browser/extensions/activity_actions.h"
#include "chrome/browser/extensions/activity_database.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"

class Profile;
using content::BrowserThread;

namespace extensions {
class Extension;

// A utility for tracing interesting activity for each extension.
// It writes to an ActivityDatabase on a separate thread to record the activity.
class ActivityLog : public ProfileKeyedService,
                    public TabHelper::ScriptExecutionObserver {
 public:
  enum Activity {
    ACTIVITY_EXTENSION_API_CALL,   // Extension API invocation is called.
    ACTIVITY_EXTENSION_API_BLOCK,  // Extension API invocation is blocked.
    ACTIVITY_CONTENT_SCRIPT,       // Content script is executing.
    ACTIVITY_EVENT_DISPATCH,       // Event sent to listener in extension.
  };

  // Observers can listen for activity events.
  class Observer {
   public:
    virtual void OnExtensionActivity(
        const Extension* extension,
        Activity activity,
        const std::string& message) = 0;
  };

  // ActivityLog is a singleton, so don't instantiate it with the constructor;
  // use GetInstance instead.
  static ActivityLog* GetInstance(Profile* profile);

  // Currently, we only want to record actions if the user has opted in to the
  // ActivityLog feature.
  static bool IsLogEnabled();

  // Recompute whether logging should be enabled (the value of IsLogEnabled is
  // normally cached).  WARNING: This may not be thread-safe, and is only
  // really intended for use by unit tests.
  static void RecomputeLoggingIsEnabled();

  // Add/remove observer.
  void AddObserver(const Extension* extension, Observer* observer);
  void RemoveObserver(const Extension* extension,
                      Observer* observer);

  // Check for the existence observer list by extension_id.
  bool HasObservers(const Extension* extension) const;

  // Log a successful API call made by an extension.
  // This will create an APIAction for storage in the database.
  // (Note: implemented as a wrapper for LogAPIActionInternal.)
  void LogAPIAction(const Extension* extension,
                    const std::string& name,    // e.g., tabs.get
                    ListValue* args,            // the argument values e.g. 46
                    const std::string& extra);  // any extra logging info

  // Log an event notification delivered to an extension.
  // This will create an APIAction for storage in the database.
  // (Note: implemented as a wrapper for LogAPIActionInternal.)
  void LogEventAction(const Extension* extension,
                      const std::string& name,    // e.g., tabs.onUpdate
                      ListValue* args,            // arguments to the callback
                      const std::string& extra);  // any extra logging info

  // Log a blocked API call made by an extension.
  // This will create a BlockedAction for storage in the database.
  void LogBlockedAction(const Extension* extension,
                        const std::string& blocked_call,  // e.g., tabs.get
                        ListValue* args,                  // argument values
                        const char* reason,               // why it's blocked
                        const std::string& extra);        // extra logging info

  // Log an interaction between an extension and a URL.
  // This will create a DOMAction for storage in the database.
  // The technical message might be the list of content scripts that have been
  // injected, or the DOM API call; it's what's shown under "More".
  void LogDOMAction(const Extension* extension,
                    const GURL& url,                      // target URL
                    const string16& url_title,            // title of the URL
                    const std::string& api_call,          // api call
                    const ListValue* args,                // arguments
                    const std::string& extra);            // extra logging info

  // Retrieves the list of actions for a given extension on a specific day.
  // Today is 0, yesterday is 1, etc. Returns one day at a time.
  // Response is sent to the method/function in the callback.
  // Use base::Bind to create the callback.
  void GetActions(const std::string& extension_id,
                  const int day,
                  const base::Callback
                      <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>&
                      callback);

  // An error has happened; we want to rollback and close the db.
  // Needs to be public so the error delegate can call it.
  void KillActivityLogDatabase();

 private:
  friend class ActivityLogFactory;

  explicit ActivityLog(Profile* profile);
  virtual ~ActivityLog();

  // We log callbacks and API calls very similarly, so we handle them the same
  // way internally.
  void LogAPIActionInternal(
      const Extension* extension,
      const std::string& api_call,
      const ListValue* args,
      const std::string& extra,
      const APIAction::Type type);

  // We log content script injection and DOM API calls using the same underlying
  // mechanism, so they have the same internal logging structure.
  void LogDOMActionInternal(const Extension* extension,
                            const GURL& url,
                            const string16& url_title,
                            const std::string& api_call,
                            const ListValue* args,
                            const std::string& extra,
                            DOMAction::DOMActionType verb);

  // TabHelper::ScriptExecutionObserver implementation.
  // Fires when a ContentScript is executed.
  virtual void OnScriptsExecuted(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

  // The callback when initializing the database.
  void OnDBInitComplete();

  static const char* ActivityToString(Activity activity);

  // The Schedule methods dispatch the calls to the database on a
  // separate thread.
  template<typename DatabaseFunc>
  void ScheduleAndForget(DatabaseFunc func) {
    if (db_.get())
      BrowserThread::PostTask(BrowserThread::DB,
                              FROM_HERE,
                              base::Bind(func, db_.get()));
  }

  template<typename DatabaseFunc, typename ArgA>
  void ScheduleAndForget(DatabaseFunc func, ArgA a) {
    if (db_.get())
      BrowserThread::PostTask(BrowserThread::DB,
                              FROM_HERE,
                              base::Bind(func, db_.get(), a));
  }

  template<typename DatabaseFunc, typename ArgA, typename ArgB>
  void ScheduleAndForget(DatabaseFunc func, ArgA a, ArgB b) {
    if (db_.get())
      BrowserThread::PostTask(BrowserThread::DB,
                              FROM_HERE,
                              base::Bind(func, db_.get(), a, b));
  }

  typedef ObserverListThreadSafe<Observer> ObserverList;
  typedef std::map<const Extension*, scoped_refptr<ObserverList> >
      ObserverMap;
  // A map of extensions to activity observers for that extension.
  ObserverMap observers_;

  // The database wrapper that does the actual database I/O.
  scoped_refptr<extensions::ActivityDatabase> db_;

  // Whether to log activity to stdout or the UI. These are set by switches.
  bool log_activity_to_stdout_;
  bool log_activity_to_ui_;

  // log_arguments controls whether to log API call arguments. By default, we
  // don't log most arguments to avoid saving too much data. In testing mode,
  // argument collection is enabled. We also whitelist some arguments for
  // collection regardless of whether this bool is true.
  bool log_arguments_;
  base::hash_set<std::string> arg_whitelist_api_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLog);
};

// Each profile has different extensions, so we keep a different database for
// each profile.
class ActivityLogFactory : public ProfileKeyedServiceFactory {
 public:
  static ActivityLog* GetForProfile(Profile* profile) {
    return static_cast<ActivityLog*>(
        GetInstance()->GetServiceForProfile(profile, true));
  }

  static ActivityLogFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ActivityLogFactory>;
  ActivityLogFactory()
      : ProfileKeyedServiceFactory("ActivityLog",
                                   ProfileDependencyManager::GetInstance()) {}
  virtual ~ActivityLogFactory() {}

  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;

  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogFactory);
};


}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_
