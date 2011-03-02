// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_H_
#pragma once

#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/profile_import_process_host.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

using importer::ImportItem;
using importer::ProfileInfo;

class ExternalProcessImporterClient;
class ImporterBridge;
class InProcessImporterBridge;
class GURL;
class Profile;
class Task;
class TemplateURL;

struct IE7PasswordInfo;

namespace history {
struct ImportedFavIconUsage;
class URLRow;
}

namespace webkit_glue {
struct PasswordForm;
}

class FirefoxProfileLock;
class Importer;

// This class hosts the importers. It enumerates profiles from other
// browsers dynamically, and controls the process of importing. When
// the import process is done, ImporterHost deletes itself.
class ImporterHost : public base::RefCountedThreadSafe<ImporterHost>,
                     public BookmarkModelObserver,
                     public NotificationObserver {
 public:
  // An interface which an object can implement to be notified of events during
  // the import process.
  class Observer {
   public:
    // Invoked when data for the specified item is about to be collected.
    virtual void ImportItemStarted(importer::ImportItem item) = 0;

    // Invoked when data for the specified item has been collected from the
    // source profile and is now ready for further processing.
    virtual void ImportItemEnded(importer::ImportItem item) = 0;

    // Invoked when the import begins.
    virtual void ImportStarted() = 0;

    // Invoked when the source profile has been imported.
    virtual void ImportEnded() = 0;

   protected:
    virtual ~Observer() {}
  };

  ImporterHost();

  // BookmarkModelObserver implementation.
  virtual void Loaded(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);

  // NotificationObserver implementation. Called when TemplateURLModel has been
  // loaded.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ShowWarningDialog() asks user to close the application that is owning the
  // lock. They can retry or skip the importing process.
  void ShowWarningDialog();

  // OnLockViewEnd() is called when user end the dialog by clicking a push
  // button. |is_continue| is true when user clicked the "Continue" button.
  void OnLockViewEnd(bool is_continue);

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

  // When in headless mode, the importer will not show the warning dialog and
  // the outcome is as if the user had canceled the import operation.
  void set_headless() {
    headless_ = true;
  }

  bool is_headless() const {
    return headless_;
  }

  void set_parent_window(gfx::NativeWindow parent_window) {
    parent_window_ = parent_window;
  }

  void SetObserver(Observer* observer);

  // A series of functions invoked at the start, during and end of the end
  // of the import process. The middle functions are notifications that the
  // harvesting of a particular source of data (specified by |item|) is under
  // way.
  virtual void ImportStarted();
  virtual void ImportItemStarted(importer::ImportItem item);
  virtual void ImportItemEnded(importer::ImportItem item);
  virtual void ImportEnded();

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

  Observer* observer_;

  // TODO(mirandac): task_ and importer_ should be private.  Can't just put
  // them there without changing the order of construct/destruct, so do this
  // after main CL has been committed.
  // The task is the process of importing settings from other browsers.
  Task* task_;

  // The importer used in the task;
  Importer* importer_;

  // Writes data from the importer back to the profile.
  scoped_refptr<ProfileWriter> writer_;

  // True if we're waiting for the model to finish loading.
  bool waiting_for_bookmarkbar_model_;

  // Have we installed a listener on the bookmark model?
  bool installed_bookmark_observer_;

  // True if source profile is readable.
  bool is_source_readable_;

  // True if UI is not to be shown.
  bool headless_;

  // Receives notification when the TemplateURLModel has loaded.
  NotificationRegistrar registrar_;

  // Parent Window to use when showing any modal dialog boxes.
  gfx::NativeWindow parent_window_;

  // Firefox profile lock.
  scoped_ptr<FirefoxProfileLock> firefox_lock_;

 private:
  // Launches the thread that starts the import task, unless bookmark or
  // template model are not yet loaded.  If load is not detected, this method
  // will be called when the loading observer sees that model loading is
  // complete.
  virtual void InvokeTaskIfDone();

  DISALLOW_COPY_AND_ASSIGN(ImporterHost);
};

// This class manages the import process.  It creates the in-process half of the
// importer bridge and the external process importer client.
class ExternalProcessImporterHost : public ImporterHost {
 public:
  ExternalProcessImporterHost();

  // Called when the BookmarkModel has finished loading. Calls InvokeTaskIfDone
  // to start importing.
  virtual void Loaded(BookmarkModel* model);

  // Methods inherited from ImporterHost.
  virtual void StartImportSettings(const importer::ProfileInfo& profile_info,
                                   Profile* target_profile,
                                   uint16 items,
                                   ProfileWriter* writer,
                                   bool first_run);

  virtual void Cancel();

 protected:
  // Launches the ExternalProcessImporterClient unless bookmark or template
  // model are not yet loaded.  If load is not detected, this method will be
  // called when the loading observer sees that model loading is complete.
  virtual void InvokeTaskIfDone();

 private:
  // Used to pass notifications from the browser side to the external process.
  ExternalProcessImporterClient* client_;

  // Data for the external importer: ------------------------------------------
  // Information about a profile needed for importing.
  const importer::ProfileInfo* profile_info_;

  // Mask of items to be imported (see importer::ImportItem).
  uint16 items_;

  // Whether to import bookmarks to the bookmark bar.
  bool import_to_bookmark_bar_;

  // True if the import process has been cancelled.
  bool cancelled_;

  // True if the import process has been launched. This prevents race
  // conditions on import cancel.
  bool import_process_launched_;

  // End of external importer data --------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterHost);
};

// This class is the client for the ProfileImportProcessHost.  It collects
// notifications from this process host and feeds data back to the importer
// host, who actually does the writing.
class ExternalProcessImporterClient
    : public ProfileImportProcessHost::ImportProcessClient {
 public:
  ExternalProcessImporterClient(ExternalProcessImporterHost* importer_host,
                                const importer::ProfileInfo& profile_info,
                                int items,
                                InProcessImporterBridge* bridge,
                                bool import_to_bookmark_bar);

  ~ExternalProcessImporterClient();

  // Launches the task to start the external process.
  virtual void Start();

  // Creates a new ProfileImportProcessHost, which launches the import process.
  virtual void StartProcessOnIOThread(ResourceDispatcherHost* rdh,
                                      BrowserThread::ID thread_id);

  // Called by the ExternalProcessImporterHost on import cancel.
  virtual void Cancel();

  // Cancel import process on IO thread.
  void CancelImportProcessOnIOThread();

  // Report item completely downloaded on IO thread.
  void NotifyItemFinishedOnIOThread(importer::ImportItem import_item);

  // Cancel import on process crash.
  virtual void OnProcessCrashed(int exit_code);

  // Notifies the importerhost that import has finished, and calls Release().
  void Cleanup();

  // ProfileImportProcessHost messages ----------------------------------------
  // The following methods are called by ProfileImportProcessHost when the
  // corresponding message has been received from the import process.
  virtual void OnImportStart();
  virtual void OnImportFinished(bool succeeded, std::string error_msg);
  virtual void OnImportItemStart(int item_data);
  virtual void OnImportItemFinished(int item_data);

  // Called on first message received when importing history; gives total
  // number of rows to be imported.
  virtual void OnHistoryImportStart(size_t total_history_rows_count);

  // Called when a group of URLRows has been received.
  // The source is passed with history::VisitSource type.
  virtual void OnHistoryImportGroup(
      const std::vector<history::URLRow> &history_rows_group,
      int visit_source);

  // Called when the home page has been received.
  virtual void OnHomePageImportReady(const GURL& home_page);

  // First message received when importing bookmarks.
  // |first_folder_name| can be NULL.
  // |options| is described in ProfileWriter::BookmarkOptions.
  // |total_bookmarks_count| is the total number of bookmarks to be imported.
  virtual void OnBookmarksImportStart(
      const std::wstring first_folder_name,
          int options, size_t total_bookmarks_count);

  // Called when a group of bookmarks has been received.
  virtual void OnBookmarksImportGroup(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks_group);

  // First message received when importing favicons.  |total_fav_icons_size|
  // gives the total number of fav icons to be imported.
  virtual void OnFavIconsImportStart(size_t total_fav_icons_count);

  // Called when a group of favicons has been received.
  virtual void OnFavIconsImportGroup(
      const std::vector<history::ImportedFavIconUsage>& fav_icons_group);

  // Called when the passwordform has been received.
  virtual void OnPasswordFormImportReady(
      const webkit_glue::PasswordForm& form);

  // Called when search engines have been received.
  virtual void OnKeywordsImportReady(
      const std::vector<TemplateURL>& template_urls,
        int default_keyword_index, bool unique_on_host_and_path);

  // End ProfileImportProcessHost messages ------------------------------------

 private:
  // These variables store data being collected from the importer until the
  // entire group has been collected and is ready to be written to the profile.
  std::vector<history::URLRow> history_rows_;
  std::vector<ProfileWriter::BookmarkEntry> bookmarks_;
  std::vector<history::ImportedFavIconUsage> fav_icons_;

  // Usually some variation on IDS_BOOKMARK_GROUP_...; the name of the folder
  // under which imported bookmarks will be placed.
  std::wstring bookmarks_first_folder_name_;

  // Determines how bookmarks should be added (ProfileWriter::BookmarkOptions).
  int bookmarks_options_;

  // Total number of bookmarks to import.
  size_t total_bookmarks_count_;

  // Total number of history items to import.
  size_t total_history_rows_count_;

  // Total number of fav icons to import.
  size_t total_fav_icons_count_;

  // Notifications received from the ProfileImportProcessHost are passed back
  // to process_importer_host_, which calls the ProfileWriter to record the
  // import data.  When the import process is done, process_importer_host_
  // deletes itself.
  ExternalProcessImporterHost* process_importer_host_;

  // Handles sending messages to the external process.  Deletes itself when
  // the external process dies (see ChildProcessHost::OnChildDied).
  ProfileImportProcessHost* profile_import_process_host_;

  // Data to be passed from the importer host to the external importer.
  const importer::ProfileInfo& profile_info_;
  int items_;
  bool import_to_bookmark_bar_;

  // Takes import data coming over IPC and delivers it to be written by the
  // ProfileWriter.  Released by ExternalProcessImporterClient in its
  // destructor.
  InProcessImporterBridge* bridge_;

  // True if import process has been cancelled.
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterClient);
};

// The base class of all importers.
class Importer : public base::RefCountedThreadSafe<Importer> {
 public:
  // All importers should implement this method by adding their
  // import logic. And it will be run in file thread by ImporterHost.
  //
  // Since we do async import, the importer should invoke
  // ImporterHost::Finished() to notify its host that import
  // stuff have been finished.
  virtual void StartImport(const importer::ProfileInfo& profile_info,
                           uint16 items,
                           ImporterBridge* bridge) = 0;

  // Cancels the import process.
  virtual void Cancel();

  void set_import_to_bookmark_bar(bool import_to_bookmark_bar) {
    import_to_bookmark_bar_ = import_to_bookmark_bar;
  }

  void set_bookmark_bar_disabled(bool bookmark_bar_disabled) {
    bookmark_bar_disabled_ = bookmark_bar_disabled;
  }

  bool bookmark_bar_disabled() {
    return bookmark_bar_disabled_;
  }

  bool cancelled() const { return cancelled_; }

 protected:
  friend class base::RefCountedThreadSafe<Importer>;

  Importer();
  virtual ~Importer();

  // Given raw image data, decodes the icon, re-sampling to the correct size as
  // necessary, and re-encodes as PNG data in the given output vector. Returns
  // true on success.
  static bool ReencodeFavicon(const unsigned char* src_data, size_t src_len,
                              std::vector<unsigned char>* png_data);

  bool import_to_bookmark_bar() const { return import_to_bookmark_bar_; }

  scoped_refptr<ImporterBridge> bridge_;

 private:
  // True if the caller cancels the import process.
  bool cancelled_;

  // True if the importer is created in the first run UI.
  bool import_to_bookmark_bar_;

  // Whether bookmark bar is disabled (not shown) for importer. This is set
  // true during first run to prevent out of process bookmark importer from
  // updating bookmark bar settings.
  bool bookmark_bar_disabled_;

  DISALLOW_COPY_AND_ASSIGN(Importer);
};

class ImporterObserver;

// Shows a UI for importing and begins importing the specified items from
// source_profile to target_profile. observer is notified when the process is
// complete, can be NULL. parent is the window to parent the UI to, can be NULL
// if there's nothing to parent to. first_run is true if it's invoked in the
// first run UI.
void StartImportingWithUI(gfx::NativeWindow parent_window,
                          uint16 items,
                          ImporterHost* coordinator,
                          const importer::ProfileInfo& source_profile,
                          Profile* target_profile,
                          ImporterObserver* observer,
                          bool first_run);

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_H_
