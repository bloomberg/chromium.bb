// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class FirefoxProfileLock;
class Importer;
class Profile;

namespace importer {
class ImporterProgressObserver;
}

// This class hosts the importers. It enumerates profiles from other
// browsers dynamically, and controls the process of importing. When
// the import process is done, ImporterHost deletes itself.
class ImporterHost : public BaseBookmarkModelObserver,
                     public content::NotificationObserver,
                     public chrome::BrowserListObserver {
 public:
  ImporterHost();

  void SetObserver(importer::ImporterProgressObserver* observer);

  // A series of functions invoked at the start, during and end of the import
  // process. The middle functions are notifications that the a harvesting of a
  // particular source of data (specified by |item|) is under way.
  void NotifyImportStarted();
  void NotifyImportItemStarted(importer::ImportItem item);
  void NotifyImportItemEnded(importer::ImportItem item);
  void NotifyImportEnded();

  // When in headless mode, the importer will not show any warning dialog if
  // a user action is required (e.g., Firefox profile is locked and user should
  // close Firefox to continue) and the outcome is as if the user had canceled
  // the import operation.
  void set_headless() { headless_ = true; }
  bool is_headless() const { return headless_; }

  void set_parent_window(gfx::NativeWindow parent_window) {
    parent_window_ = parent_window;
  }

  void set_browser(Browser* browser) { browser_ = browser; }

  // Starts the process of importing the settings and data depending on what the
  // user selected.
  // |source_profile| - importer profile to import.
  // |target_profile| - profile to import into.
  // |items| - specifies which data to import (bitmask of importer::ImportItem).
  // |writer| - called to actually write data back to the profile.
  virtual void StartImportSettings(
      const importer::SourceProfile& source_profile,
      Profile* target_profile,
      uint16 items,
      ProfileWriter* writer);

 protected:
  virtual ~ImporterHost();

  // ShowWarningDialog() asks user to close the application that is owning the
  // lock. They can retry or skip the importing process.
  void ShowWarningDialog();

  // This is called when when user ends the lock dialog by clicking on either
  // the "Skip" or "Continue" buttons. |is_continue| is true when user clicked
  // the "Continue" button.
  void OnImportLockDialogEnd(bool is_continue);

  // Make sure that Firefox isn't running, if import browser is Firefox. Show
  // to the user a dialog that notifies that is necessary to close Firefox
  // prior to continue.
  // |source_profile| - importer profile to import.
  // |items| - specifies which data to import (bitmask of importer::ImportItem).
  void CheckForFirefoxLock(const importer::SourceProfile& source_profile,
                           uint16 items);

  // Make sure BookmarkModel and TemplateURLService are loaded before import
  // process starts, if bookmarks and/or search engines are among the items
  // which are to be imported.
  void CheckForLoadedModels(uint16 items);

  // Vends weak pointers for the importer to call us back.
  base::WeakPtrFactory<ImporterHost> weak_ptr_factory_;

  // Profile we're importing from.
  Profile* profile_;

  // True if we're waiting for the model to finish loading.
  bool waiting_for_bookmarkbar_model_;

  // Have we installed a listener on the bookmark model?
  bool installed_bookmark_observer_;

  // True if source profile is readable.
  bool is_source_readable_;

  // Receives notification when the TemplateURLService has loaded.
  content::NotificationRegistrar registrar_;

  // Writes data from the importer back to the profile.
  scoped_refptr<ProfileWriter> writer_;

 private:
  friend class base::RefCountedThreadSafe<ImporterHost>;

  // Launches the thread that starts the import task, unless bookmark or
  // template model are not yet loaded. If load is not detected, this method
  // will be called when the loading observer sees that model loading is
  // complete.
  virtual void InvokeTaskIfDone();

  // Called when IsGoogleGAIACookieInstalled is done.
  void OnGoogleGAIACookieChecked(bool result);

  // BaseBookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkModelChanged() OVERRIDE;

  // content::NotificationObserver:
  // Called when TemplateURLService has been loaded.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // chrome::BrowserListObserver
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // The task is the process of importing settings from other browsers.
  base::Closure task_;

  // The importer used in the task.
  scoped_refptr<Importer> importer_;

  // True if UI is not to be shown.
  bool headless_;

  // Parent window that we pass to the import lock dialog (i.e, the Firefox
  // warning dialog).
  gfx::NativeWindow parent_window_;

  // Used to add a new tab if we need the user to sign in.
  Browser* browser_;

  // The observer that we need to notify about changes in the import process.
  importer::ImporterProgressObserver* observer_;

  // Firefox profile lock.
  scoped_ptr<FirefoxProfileLock> firefox_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImporterHost);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_
