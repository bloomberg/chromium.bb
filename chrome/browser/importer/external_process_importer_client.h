// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_CLIENT_H_
#define CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/profile_import_process_host.h"

class ExternalProcessImporterHost;
class InProcessImporterBridge;

namespace history {
class URLRow;
struct ImportedFavIconUsage;
}

namespace importer {
struct ProfileInfo;
}

// This class is the client for the ProfileImportProcessHost.  It collects
// notifications from this process host and feeds data back to the importer
// host, who actually does the writing.
class ExternalProcessImporterClient
    : public ProfileImportProcessHost::ImportProcessClient {
 public:
  ExternalProcessImporterClient(ExternalProcessImporterHost* importer_host,
                                const importer::ProfileInfo& profile_info,
                                uint16 items,
                                InProcessImporterBridge* bridge,
                                bool import_to_bookmark_bar);
  virtual ~ExternalProcessImporterClient();

  // Cancel import process on IO thread.
  void CancelImportProcessOnIOThread();

  // Report item completely downloaded on IO thread.
  void NotifyItemFinishedOnIOThread(importer::ImportItem import_item);

  // Notifies the importerhost that import has finished, and calls Release().
  void Cleanup();

  // Launches the task to start the external process.
  virtual void Start();

  // Creates a new ProfileImportProcessHost, which launches the import process.
  virtual void StartProcessOnIOThread(ResourceDispatcherHost* rdh,
                                      BrowserThread::ID thread_id);

  // Called by the ExternalProcessImporterHost on import cancel.
  virtual void Cancel();

  // Begin ProfileImportProcessHost::ImportProcessClient implementation.
  virtual void OnProcessCrashed(int exit_status) OVERRIDE;
  virtual void OnImportStart() OVERRIDE;
  virtual void OnImportFinished(bool succeeded, std::string error_msg) OVERRIDE;
  virtual void OnImportItemStart(int item) OVERRIDE;
  virtual void OnImportItemFinished(int item) OVERRIDE;

  // Called on first message received when importing history; gives total
  // number of rows to be imported.
  virtual void OnHistoryImportStart(size_t total_history_rows_count) OVERRIDE;

  // Called when a group of URLRows has been received.
  // The source is passed with history::VisitSource type.
  virtual void OnHistoryImportGroup(
      const std::vector<history::URLRow> &history_rows_group,
      int visit_source) OVERRIDE;

  // Called when the home page has been received.
  virtual void OnHomePageImportReady(const GURL& home_page) OVERRIDE;

  // First message received when importing bookmarks.
  // |first_folder_name| can be NULL.
  // |options| is described in ProfileWriter::BookmarkOptions.
  // |total_bookmarks_count| is the total number of bookmarks to be imported.
  virtual void OnBookmarksImportStart(const std::wstring first_folder_name,
                                      int options,
                                      size_t total_bookmarks_count) OVERRIDE;

  // Called when a group of bookmarks has been received.
  virtual void OnBookmarksImportGroup(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks_group)
          OVERRIDE;

  // First message received when importing favicons. |total_fav_icons_size|
  // gives the total number of favicons to be imported.
  virtual void OnFavIconsImportStart(size_t total_fav_icons_count) OVERRIDE;

  // Called when a group of favicons has been received.
  virtual void OnFavIconsImportGroup(
      const std::vector<history::ImportedFavIconUsage>& fav_icons_group)
          OVERRIDE;

  // Called when the passwordform has been received.
  virtual void OnPasswordFormImportReady(
      const webkit_glue::PasswordForm& form) OVERRIDE;

  // Called when search engines have been received.
  virtual void OnKeywordsImportReady(
      const std::vector<TemplateURL>& template_urls,
      int default_keyword_index,
      bool unique_on_host_and_path) OVERRIDE;

  // End ProfileImportProcessHost::ImportProcessClient implementation.

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

  // Total number of favicons to import.
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
  uint16 items_;
  bool import_to_bookmark_bar_;

  // Takes import data coming over IPC and delivers it to be written by the
  // ProfileWriter.  Released by ExternalProcessImporterClient in its
  // destructor.
  InProcessImporterBridge* bridge_;

  // True if import process has been cancelled.
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterClient);
};

#endif  // CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_CLIENT_H_
