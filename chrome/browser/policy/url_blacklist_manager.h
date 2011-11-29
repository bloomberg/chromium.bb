// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
#define CHROME_BROWSER_POLICY_URL_BLACKLIST_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

class GURL;
class PrefService;

namespace policy {

// Contains a set of filters to block and allow certain URLs, and matches GURLs
// against this set. The filters are currently kept in memory.
class URLBlacklist {
 public:
  // A constant mapped to a scheme that can be filtered.
  enum SchemeFlag {
    SCHEME_HTTP   = 1 << 0,
    SCHEME_HTTPS  = 1 << 1,
    SCHEME_FTP    = 1 << 2,

    SCHEME_ALL    = (1 << 3) - 1,
  };

  URLBlacklist();
  virtual ~URLBlacklist();

  // URLs matching |filter| will be blocked. The filter format is documented
  // at http://www.chromium.org/administrators/url-blacklist-filter-format
  void Block(const std::string& filter);

  // URLs matching |filter| will be allowed. If |filter| is both Blocked and
  // Allowed, Allow takes precedence.
  void Allow(const std::string& filter);

  // Returns true if the URL is blocked.
  bool IsURLBlocked(const GURL& url) const;

  // Returns true if |scheme| is a scheme that can be filtered. Returns true
  // and sets |flag| to SCHEME_ALL if |scheme| is empty.
  static bool SchemeToFlag(const std::string& scheme, SchemeFlag* flag);

  // Splits a URL filter into its components. A GURL isn't used because these
  // can be invalid URLs e.g. "google.com".
  // Returns false if the URL couldn't be parsed.
  // The optional username and password are ignored.
  // |port| is 0 if none is explicitly defined.
  // |path| does not include query parameters.
  static bool FilterToComponents(const std::string& filter,
                                 std::string* scheme,
                                 std::string* host,
                                 uint16* port,
                                 std::string* path);
 private:
  struct PathFilter;

  typedef std::vector<PathFilter> PathFilterList;
  typedef base::hash_map<std::string, PathFilterList*> HostFilterTable;

  void AddFilter(const std::string& filter, bool block);

  HostFilterTable host_filters_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklist);
};

// Tracks the blacklist policies for a given profile, and updates it on changes.
//
// This class interacts with both the UI thread, where notifications of pref
// changes are received from, and the IO thread, which owns it (in the
// ProfileIOData) and checks for blacklisted URLs (from ChromeNetworkDelegate).
//
// It must be constructed on the UI thread, to set up |ui_weak_ptr_factory_| and
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
class URLBlacklistManager : public content::NotificationObserver {
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

  // Used to delay updating the blacklist while the preferences are
  // changing, and execute only one update per simultaneous prefs changes.
  void ScheduleUpdate();

  // Updates the blacklist using the current preference values.
  // Virtual for testing.
  virtual void Update();

  // Starts the blacklist update on the IO thread, using the filters in
  // |block| and |allow|. Protected for testing.
  void UpdateOnIO(StringVector* block, StringVector* allow);

 private:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ---------
  // UI thread
  // ---------

  // Used to post update tasks to the UI thread.
  base::WeakPtrFactory<URLBlacklistManager> ui_weak_ptr_factory_;

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
