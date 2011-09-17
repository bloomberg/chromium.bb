// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
#define CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/autocomplete/shortcuts_provider_shortcut.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace history {

// This class manages the shortcut provider backend - access to database on the
// db thread, etc.
class ShortcutsBackend : public base::RefCountedThreadSafe<ShortcutsBackend>,
                         public NotificationObserver {
 public:
  // |profile| is necessary for profile notifications only and can be NULL in
  // unit-tests. |db_folder_path| could be an empty path only in unit-tests as
  // well. It means there is no database created, all things are done in memory.
  ShortcutsBackend(const FilePath& db_folder_path, Profile* profile);
  virtual ~ShortcutsBackend();

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
  bool AddShortcut(const shortcuts_provider::Shortcut& shortcut);

  // Updates timing and selection count for the Shortcut.
  bool UpdateShortcut(const shortcuts_provider::Shortcut& shortcut);

  // Deletes the Shortcuts with the id.
  bool DeleteShortcutsWithIds(const std::vector<std::string>& shortcut_ids);

  // Deletes the Shortcuts with the url.
  bool DeleteShortcutsWithUrl(const GURL& shortcut_url);

  // Deletes all of the shortcuts.
  bool DeleteAllShortcuts();

  const shortcuts_provider::ShortcutMap& shortcuts_map() const {
    return shortcuts_map_;
  }

  const shortcuts_provider::GuidToShortcutsIteratorMap& guid_map() const {
    return guid_map_;
  }

  void AddObserver(ShortcutsBackendObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(ShortcutsBackendObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

 private:
  // Internal initialization of the back-end. Posted by Init() to the DB thread.
  // On completion posts InitCompleted() back to UI thread.
  void InitInternal();

  // Finishes initialization on UI thread, notifies all observers.
  void InitCompleted();

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

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
  scoped_ptr<shortcuts_provider::ShortcutMap> temp_shortcuts_map_;
  scoped_ptr<shortcuts_provider::GuidToShortcutsIteratorMap> temp_guid_map_;

  shortcuts_provider::ShortcutMap shortcuts_map_;
  // This is a helper map for quick access to a shortcut by guid.
  shortcuts_provider::GuidToShortcutsIteratorMap guid_map_;

  NotificationRegistrar notification_registrar_;

  // For some unit-test only.
  bool no_db_access_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
