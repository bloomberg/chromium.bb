// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
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
    ACTIVITY_CONTENT_SCRIPT        // Content script is executing.
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

  // Add/remove observer.
  void AddObserver(const Extension* extension, Observer* observer);
  void RemoveObserver(const Extension* extension,
                      Observer* observer);

  // Check for the existence observer list by extension_id.
  bool HasObservers(const Extension* extension) const;

  // Log a successful API call made by an extension.
  // This will create an APIAction for storage in the database.
  void LogAPIAction(const Extension* extension,
                    const std::string& name,    // e.g., chrome.tabs.get
                    const ListValue* args,      // the argument values e.g. 46
                    const std::string& extra);  // any extra logging info

  // Log a blocked API call made by an extension.
  // This will create a BlockedAction for storage in the database.
  void LogBlockedAction(const Extension* extension,
                        const std::string& blocked_call,  // eg chrome.tabs.get
                        const ListValue* args,            // argument values
                        const char* reason,               // why it's blocked
                        const std::string& extra);        // extra logging info

  // Log an interaction between an extension and a URL.
  // This will create a UrlAction for storage in the database.
  // The technical message might be the list of content scripts that have been
  // injected, or the DOM API call; it's what's shown under "More".
  void LogUrlAction(const Extension* extension,
                    const UrlAction::UrlActionType verb,  // eg XHR
                    const GURL& url,                      // target URL
                    const string16& url_title,            // title of the URL,
                                                          // can be empty string
                    const std::string& technical_message, // "More"
                    const std::string& extra);            // extra logging info

  // An error has happened; we want to rollback and close the db.
  // Needs to be public so the error delegate can call it.
  void KillActivityLogDatabase();

 private:
  friend class ActivityLogFactory;

  explicit ActivityLog(Profile* profile);
  virtual ~ActivityLog();

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

  // Whether to log activity to stdout. This is set by checking the
  // enable-extension-activity-logging switch.
  bool log_activity_to_stdout_;

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
