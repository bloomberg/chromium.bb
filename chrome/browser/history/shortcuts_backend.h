// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
#define CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list_threadsafe.h"
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
  // unit-tests.
  ShortcutsBackend(const FilePath& db_folder_path, Profile* profile);
  virtual ~ShortcutsBackend();

  // The interface is guaranteed to be called on the thread AddObserver()
  // was called.
  class ShortcutsBackendObserver {
   public:
    // Called after the database is loaded and Init() completed.
    virtual void OnShortcutsLoaded() = 0;
    // This callback is called when addition or change was processed by the
    // database.
    virtual void OnShortcutAddedOrUpdated(
        const shortcuts_provider::Shortcut& shortcut) = 0;
    // Called when shortcuts are removed from the database.
    virtual void OnShortcutsRemoved(
        const std::vector<std::string>& shortcut_ids) = 0;
   protected:
    virtual ~ShortcutsBackendObserver() {}
  };

  // Asynchronously initializes the ShortcutsBackend, it is safe to call
  // multiple times - only the first call will be processed.
  bool Init();

  bool initialized() const { return current_state_ == INITIALIZED; }

  // Adds the Shortcut to the database.
  bool AddShortcut(shortcuts_provider::Shortcut shortcut);

  // Updates timing and selection count for the Shortcut.
  bool UpdateShortcut(shortcuts_provider::Shortcut shortcut);

  // Deletes the Shortcuts with the id.
  bool DeleteShortcutsWithIds(const std::vector<std::string>& shortcut_ids);

  // Deletes the Shortcuts with the url.
  bool DeleteShortcutsWithUrl(const GURL& shortcut_url);

  // Replaces the contents of |shortcuts| with the backend's from the database.
  bool GetShortcuts(shortcuts_provider::ShortcutMap* shortcuts);

  void AddObserver(ShortcutsBackendObserver* obs) {
    observer_list_->AddObserver(obs);
  }

  void RemoveObserver(ShortcutsBackendObserver* obs) {
    observer_list_->RemoveObserver(obs);
  }

 private:
  // Internal initialization of the back-end. Posted by Init() to the DB thread.
  // On completion notifies all back-end observers asyncronously on their own
  // thread.
  void InitInternal();
  // Internal addition or update of a shortcut. Posted by AddShortcut() or
  // UpdateShortcut() to the DB thread. On completion notifies all back-end
  // observers asyncronously on their own thread.
  void AddOrUpdateShortcutInternal(shortcuts_provider::Shortcut shortcut,
                                   bool add);
  // Internal deletion of shortcuts. Posted by DeleteShortcutsWithIds() to the
  // DB thread. On completion notifies all back-end observers asyncronously on
  // their own thread.
  void DeleteShortcutsWithIdsInternal(std::vector<std::string> shortcut_ids);
  // Internal deletion of shortcut with the url. Posted by
  // DeleteShortcutsWithUrl() to the DB thread. On completion notifies all
  // back-end observers asyncronously on their own thread.
  void DeleteShortcutsWithUrlInternal(std::string shortcut_url);

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
  // |current_state_| has values of CurrentState enum.
  volatile base::subtle::Atomic32 current_state_;
  scoped_refptr<ObserverListThreadSafe<ShortcutsBackendObserver> >
      observer_list_;
  ShortcutsDatabase db_;

  shortcuts_provider::ShortcutMap shortcuts_map_;
  // This is a helper map for quick access to a shortcut by guid.
  shortcuts_provider::GuidToShortcutsIteratorMap guid_map_;

  base::Lock data_access_lock_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ShortcutsBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_SHORTCUTS_BACKEND_H_
