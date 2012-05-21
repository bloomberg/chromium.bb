// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTIVITY_LOG_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTIVITY_LOG_H_

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"

namespace extensions {
class Extension;
}

// A utility for tracing interesting activity for each extension.
class ExtensionActivityLog {
 public:
  enum Activity {
    ACTIVITY_EXTENSION_API_CALL,  // Extension API invocation is called.
    ACTIVITY_EXTENSION_API_BLOCK  // Extension API invocation is blocked.
  };

  // Observers can listen for activity events.
  class Observer {
   public:
    virtual void OnExtensionActivity(const extensions::Extension* extension,
                                     Activity activity,
                                     const std::string& msg) = 0;
  };

  ~ExtensionActivityLog();
  static ExtensionActivityLog* GetInstance();

  // Add/remove observer.
  void AddObserver(const extensions::Extension* extension, Observer* observer);
  void RemoveObserver(const extensions::Extension* extension,
                      Observer* observer);

  // Check for the existence observer list by extension_id.
  bool HasObservers(const extensions::Extension* extension) const;

  // Log |activity| for |extension|.
  void Log(const extensions::Extension* extension,
           Activity activity,
           const std::string& msg) const;

 private:
  ExtensionActivityLog();
  friend struct DefaultSingletonTraits<ExtensionActivityLog>;

  static const char* ActivityToString(Activity activity);

  // A lock used to synchronize access to member variables.
  mutable base::Lock lock_;

  // Whether to log activity to stdout. This is set by checking the
  // enable-extension-activity-logging switch.
  bool log_activity_to_stdout_;

  typedef ObserverListThreadSafe<Observer> ObserverList;
  typedef std::map<const extensions::Extension*, scoped_refptr<ObserverList> >
      ObserverMap;
  // A map of extensions to activity observers for that extension.
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActivityLog);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTIVITY_LOG_H_
