// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/tab_helper.h"

namespace extensions {
class Extension;

// A utility for tracing interesting activity for each extension.
class ActivityLog : public TabHelper::ScriptExecutionObserver {
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
        const std::vector<std::string>& messages) = 0;
  };

  virtual ~ActivityLog();
  static ActivityLog* GetInstance();

  // Add/remove observer.
  void AddObserver(const Extension* extension, Observer* observer);
  void RemoveObserver(const Extension* extension,
                      Observer* observer);

  // Check for the existence observer list by extension_id.
  bool HasObservers(const Extension* extension) const;

  // Log |activity| for |extension|.
  void Log(const Extension* extension,
           Activity activity,
           const std::string& message) const;
  void Log(const Extension* extension,
           Activity activity,
           const std::vector<std::string>& messages) const;

 private:
  ActivityLog();
  friend struct DefaultSingletonTraits<ActivityLog>;

  // TabHelper::ScriptExecutionObserver implementation.
  virtual void OnScriptsExecuted(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& extension_ids,
      int32 page_id,
      const GURL& on_url) OVERRIDE;

  static const char* ActivityToString(Activity activity);

  // A lock used to synchronize access to member variables.
  mutable base::Lock lock_;

  // Whether to log activity to stdout. This is set by checking the
  // enable-extension-activity-logging switch.
  bool log_activity_to_stdout_;

  typedef ObserverListThreadSafe<Observer> ObserverList;
  typedef std::map<const Extension*, scoped_refptr<ObserverList> >
      ObserverMap;
  // A map of extensions to activity observers for that extension.
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLog);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_H_
