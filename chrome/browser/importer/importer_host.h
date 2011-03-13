// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

class ExternalProcessImporterClient;
class FirefoxProfileLock;
class GURL;
class Importer;
class InProcessImporterBridge;
class Profile;
class Task;
class TemplateURL;

namespace history {
class URLRow;
struct ImportedFavIconUsage;
}

namespace importer {
class ImporterProgressObserver;
}

namespace webkit_glue {
struct PasswordForm;
}

// This class hosts the importers. It enumerates profiles from other
// browsers dynamically, and controls the process of importing. When
// the import process is done, ImporterHost deletes itself.
class ImporterHost : public base::RefCountedThreadSafe<ImporterHost>,
                     public BaseBookmarkModelObserver,
                     public NotificationObserver {
 public:
  ImporterHost();

  // ShowWarningDialog() asks user to close the application that is owning the
  // lock. They can retry or skip the importing process.
  void ShowWarningDialog();

  // This is called when when user ends the lock dialog by clicking on either
  // the "Skip" or "Continue" buttons. |is_continue| is true when user clicked
  // the "Continue" button.
  void OnImportLockDialogEnd(bool is_continue);

  // Starts the process of importing the settings and data depending on what
  // the user selected.
  // |profile_info| -- browser profile to import.
  // |target_profile| -- profile to import into.
  // |items| -- specifies which data to import (mask of ImportItems).
  // |writer| -- called to actually write data back to the profile.
  // |first_run| -- true if this method is being called during first run.
  virtual void StartImportSettings(const importer::ProfileInfo& profile_info,
                                   Profile* target_profile,
                                   uint16 items,
                                   ProfileWriter* writer,
                                   bool first_run);

  // Cancel import.
  virtual void Cancel();

  void SetObserver(importer::ImporterProgressObserver* observer);

  // A series of functions invoked at the start, during and end of the import
  // process. The middle functions are notifications that the a harvesting of a
  // particular source of data (specified by |item|) is under way.
  void NotifyImportStarted();
  void NotifyImportItemStarted(importer::ImportItem item);
  void NotifyImportItemEnded(importer::ImportItem item);
  void NotifyImportEnded();

  // When in headless mode, the importer will not show the warning dialog and
  // the outcome is as if the user had canceled the import operation.
  void set_headless() { headless_ = true; }
  bool is_headless() const { return headless_; }

  void set_parent_window(gfx::NativeWindow parent_window) {
    parent_window_ = parent_window;
  }

 protected:
  friend class base::RefCountedThreadSafe<ImporterHost>;

  ~ImporterHost();

  // Returns true if importer should import to bookmark bar.
  bool ShouldImportToBookmarkBar(bool first_run);

  // Make sure that Firefox isn't running, if import browser is Firefox. Show
  // the user a dialog to notify that they need to close FF to continue.
  // |profile_info| holds the browser type and source path.
  // |items| is a mask of all ImportItems that are to be imported.
  // |first_run| is true if this method is being called during first run.
  void CheckForFirefoxLock(const importer::ProfileInfo& profile_info,
                           uint16 items, bool first_run);

  // Make sure BookmarkModel and TemplateURLModel are loaded before import
  // process starts, if bookmarks and / or search engines are among the items
  // which are to be imported.
  void CheckForLoadedModels(uint16 items);

  // Profile we're importing from.
  Profile* profile_;

  // TODO(mirandac): task_ and importer_ should be private.  Can't just put
  // them there without changing the order of construct/destruct, so do this
  // after main CL has been committed.
  // The task is the process of importing settings from other browsers.
  Task* task_;

  // The importer used in the task;
  Importer* importer_;

  // True if we're waiting for the model to finish loading.
  bool waiting_for_bookmarkbar_model_;

  // Have we installed a listener on the bookmark model?
  bool installed_bookmark_observer_;

  // True if source profile is readable.
  bool is_source_readable_;

  // Receives notification when the TemplateURLModel has loaded.
  NotificationRegistrar registrar_;

  // Writes data from the importer back to the profile.
  scoped_refptr<ProfileWriter> writer_;

 private:
  // Launches the thread that starts the import task, unless bookmark or
  // template model are not yet loaded.  If load is not detected, this method
  // will be called when the loading observer sees that model loading is
  // complete.
  virtual void InvokeTaskIfDone();

  // BaseBookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkModelChanged() OVERRIDE;

  // NotificationObserver:
  // Called when TemplateURLModel has been loaded.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // True if UI is not to be shown.
  bool headless_;

  // Parent Window to use when showing any modal dialog boxes.
  gfx::NativeWindow parent_window_;

  // The observer that we need to notify about changes in the import process.
  importer::ImporterProgressObserver* observer_;

  // Firefox profile lock.
  scoped_ptr<FirefoxProfileLock> firefox_lock_;

  DISALLOW_COPY_AND_ASSIGN(ImporterHost);
};


#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_HOST_H_
