// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
#define CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace history {

class ShortcutsDatabase;

// This class manages the shortcut provider backend - access to database on the
// db thread, etc.
class ShortcutsBackend : public RefcountedProfileKeyedService,
                         public content::NotificationObserver {
 public:
  // The following struct encapsulates one previously selected omnibox shortcut.
  struct Shortcut {
    Shortcut(const std::string& id,
             const string16& text,
             const GURL& url,
             const string16& contents,
             const ACMatchClassifications& contents_class,
             const string16& description,
             const ACMatchClassifications& description_class,
             const base::Time& last_access_time,
             int number_of_hits);
    // Required for STL, we don't use this directly.
    Shortcut();
    ~Shortcut();

    std::string id;  // Unique guid for the shortcut.
    string16 text;   // The user's original input string.
    GURL url;        // The corresponding destination URL.

    // Contents and description from the original match, along with their
    // corresponding markup. We need these in order to correctly mark which
    // parts are URLs, dim, etc. However, we strip all MATCH classifications
    // from these since we'll mark the matching portions ourselves as we match
    // the user's current typing against these Shortcuts.
    string16 contents;
    ACMatchClassifications contents_class;
    string16 description;
    ACMatchClassifications description_class;

    base::Time last_access_time;  // Last time shortcut was selected.
    int number_of_hits;           // How many times shortcut was selected.
  };

  typedef std::multimap<string16, ShortcutsBackend::Shortcut> ShortcutMap;

  // |profile| is necessary for profile notifications only and can be NULL in
  // unit-tests. For unit testing, set |suppress_db| to true to prevent creation
  // of the database, in which case all operations are performed in memory only.
  ShortcutsBackend(Profile* profile, bool suppress_db);

  // The interface is guaranteed to be called on the thread AddObserver()
  // was called.
  class ShortcutsBackendObserver {
   public:
    // Called after the database is loaded and Init() completed.
    virtual void OnShortcutsLoaded() = 0;
    // Called when shortcuts changed (added/updated/removed) in the database.
    virtual void OnShortcutsChanged() {}

   protected:
    virtual ~ShortcutsBackendObserver() {}
  };

  // Asynchronously initializes the ShortcutsBackend, it is safe to call
  // multiple times - only the first call will be processed.
  bool Init();

  bool initialized() const { return current_state_ == INITIALIZED; }

  // All of the public functions *must* be called on UI thread only!

  // Adds the Shortcut to the database.
  bool AddShortcut(const ShortcutsBackend::Shortcut& shortcut);

  // Updates timing and selection count for the Shortcut.
  bool UpdateShortcut(const ShortcutsBackend::Shortcut& shortcut);

  // Deletes the Shortcuts with the id.
  bool DeleteShortcutsWithIds(const std::vector<std::string>& shortcut_ids);

  // Deletes the Shortcuts with the url.
  bool DeleteShortcutsWithUrl(const GURL& shortcut_url);

  // Deletes all of the shortcuts.
  bool DeleteAllShortcuts();

  const ShortcutMap& shortcuts_map() const {
    return shortcuts_map_;
  }

  void AddObserver(ShortcutsBackendObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(ShortcutsBackendObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

 private:
  friend class base::RefCountedThreadSafe<ShortcutsBackend>;

  typedef std::map<std::string, ShortcutMap::iterator>
      GuidToShortcutsIteratorMap;

  virtual ~ShortcutsBackend();

  // Internal initialization of the back-end. Posted by Init() to the DB thread.
  // On completion posts InitCompleted() back to UI thread.
  void InitInternal();

  // Finishes initialization on UI thread, notifies all observers.
  void InitCompleted();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // RefcountedProfileKeyedService
  virtual void ShutdownOnUIThread() OVERRIDE;

  enum CurrentState {
    NOT_INITIALIZED,  // Backend created but not initialized.
    INITIALIZING,  // Init() called, but not completed yet.
    INITIALIZED,  // Initialization completed, all accessors can be safely
                  // called.
  };

  CurrentState current_state_;
  ObserverList<ShortcutsBackendObserver> observer_list_;
  scoped_refptr<ShortcutsDatabase> db_;

  // The |temp_shortcuts_map_| and |temp_guid_map_| used for temporary storage
  // between InitInternal() and InitComplete() to avoid doing a potentially huge
  // copy.
  scoped_ptr<ShortcutMap> temp_shortcuts_map_;
  scoped_ptr<GuidToShortcutsIteratorMap> temp_guid_map_;

  ShortcutMap shortcuts_map_;
  // This is a helper map for quick access to a shortcut by guid.
  GuidToShortcutsIteratorMap guid_map_;

  content::NotificationRegistrar notification_registrar_;

  // For some unit-test only.
  bool no_db_access_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
