// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
#define CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"

class GURL;
class NotificationDetails;
class NotificationSource;
class PrefService;

namespace base {
class ListValue;
}

namespace policy {

// TODO(joaodasilva): this is a work in progress. http://crbug.com/49612
class URLBlacklist {
 public:
  URLBlacklist();
  virtual ~URLBlacklist();

 private:
  DISALLOW_COPY_AND_ASSIGN(URLBlacklist);
};

// Tracks the blacklist policies for a given profile, and updates it on changes.
//
// This class interacts with both the UI thread, where notifications of pref
// changes are received from, and the IO thread, which owns it (in the
// ProfileIOData) and checks for blacklisted URLs (from ChromeNetworkDelegate).
//
// It must be constructed on the UI thread, to set up |ui_method_factory_| and
// the prefs listeners.
//
// ShutdownOnUIThread must be called from UI before destruction, to release
// the prefs listeners on the UI thread. This is done from ProfileIOData.
//
// Update tasks from the UI thread can post safely to the IO thread, since the
// destruction order of Profile and ProfileIOData guarantees that if this
// exists in UI, then a potential destruction on IO will come after any task
// posted to IO from that method on UI. This is used to go through IO before
// the actual update starts, and grab a WeakPtr.
class URLBlacklistManager : public NotificationObserver {
 public:
  // Must be constructed on the UI thread.
  explicit URLBlacklistManager(PrefService* pref_service);
  virtual ~URLBlacklistManager();

  // Must be called on the UI thread, before destruction.
  void ShutdownOnUIThread();

  // Returns true if |url| is blocked by the current blacklist. Must be called
  // from the IO thread.
  bool IsURLBlocked(const GURL& url) const;

  // Replaces the current blacklist. Must be called on the IO thread.
  void SetBlacklist(URLBlacklist* blacklist);

  // Registers the preferences related to blacklisting in the given PrefService.
  static void RegisterPrefs(PrefService* pref_service);

 protected:
  typedef std::vector<std::string> StringVector;

  // These are used to delay updating the blacklist while the preferences are
  // changing, and execute only one update per simultaneous prefs changes.
  void ScheduleUpdate();
  virtual void PostUpdateTask(Task* task);  // Virtual for testing.
  virtual void Update();  // Virtual for testing.

  // Starts the blacklist update on the IO thread, using the filters in
  // |block| and |allow|. Protected for testing.
  void UpdateOnIO(StringVector* block, StringVector* allow);

 private:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // ---------
  // UI thread
  // ---------

  // Used to post update tasks to the UI thread.
  ScopedRunnableMethodFactory<URLBlacklistManager> ui_method_factory_;

  // Used to track the policies and update the blacklist on changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.

  // ---------
  // IO thread
  // ---------

  // Used to get |weak_ptr_| to self on the IO thread.
  base::WeakPtrFactory<URLBlacklistManager> io_weak_ptr_factory_;

  // The current blacklist.
  scoped_ptr<URLBlacklist> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
