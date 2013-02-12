// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

class Extension;
class ExtensionPrefs;

// A blacklist of extensions.
class Blacklist : public content::NotificationObserver,
                  public base::SupportsWeakPtr<Blacklist> {
 public:
  class Observer {
   public:
    // Observes |blacklist| on construction and unobserves on destruction.
    explicit Observer(Blacklist* blacklist);

    virtual void OnBlacklistUpdated() = 0;

   protected:
    virtual ~Observer();

   private:
    Blacklist* blacklist_;
  };

  class ScopedDatabaseManagerForTest {
   public:
    explicit ScopedDatabaseManagerForTest(
        scoped_refptr<SafeBrowsingDatabaseManager> database_manager);

    ~ScopedDatabaseManagerForTest();

   private:
    scoped_refptr<SafeBrowsingDatabaseManager> original_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDatabaseManagerForTest);
  };

  typedef base::Callback<void(const std::set<std::string>&)>
      GetBlacklistedIDsCallback;

  typedef base::Callback<void(bool)> IsBlacklistedCallback;

  // |prefs_| must outlive this.
  explicit Blacklist(ExtensionPrefs* prefs);

  virtual ~Blacklist();

  // From the set of extension IDs passed in via |ids|, asynchronously checks
  // which are blacklisted and includes them in the resulting set passed
  // via |callback|, which will be sent on the caller's message loop.
  //
  // For a synchronous version which ONLY CHECKS CURRENTLY INSTALLED EXTENSIONS
  // see ExtensionPrefs::IsExtensionBlacklisted.
  void GetBlacklistedIDs(const std::set<std::string>& ids,
                         const GetBlacklistedIDsCallback& callback);

  // More convenient form of GetBlacklistedIDs for checking a single extension.
  void IsBlacklisted(const std::string& extension_id,
                     const IsBlacklistedCallback& callback);

  // Sets the blacklist from the updater to contain the extension IDs in |ids|
  void SetFromUpdater(const std::vector<std::string>& ids,
                      const std::string& version);

  // Adds/removes an observer to the blacklist.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Use via ScopedDatabaseManagerForTest.
  static void SetDatabaseManager(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager);
  static scoped_refptr<SafeBrowsingDatabaseManager> GetDatabaseManager();

  // Handles the |safebrowsing_blacklisted_ids| response from querying the
  // safebrowsing blacklist, given that we know |pref_blacklisted_ids| are
  // already blacklisted. Responds to |callback| with the union.
  void OnSafeBrowsingResponse(
      const std::set<std::string>& pref_blacklisted_ids,
      const GetBlacklistedIDsCallback& callback,
      const std::set<std::string>& safebrowsing_blacklisted_ids);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  ObserverList<Observer> observers_;

  ExtensionPrefs* const prefs_;

  std::set<std::string> prefs_blacklist_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(Blacklist);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
