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
#include "base/string16.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/browser/browser_thread.h"
#include "content/browser/utility_process_host.h"

class ExternalProcessImporterHost;
class InProcessImporterBridge;
class UtilityProcessHost;

namespace history {
class URLRow;
struct ImportedFaviconUsage;
}

// This class is the client for the out of process profile importing.  It
// collects notifications from this process host and feeds data back to the
// importer host, who actually does the writing.
class ExternalProcessImporterClient : public UtilityProcessHost::Client {
 public:
  ExternalProcessImporterClient(ExternalProcessImporterHost* importer_host,
                                const importer::SourceProfile& source_profile,
                                uint16 items,
                                InProcessImporterBridge* bridge);
  virtual ~ExternalProcessImporterClient();

  // Cancel import process on IO thread.
  void CancelImportProcessOnIOThread();

  // Report item completely downloaded on IO thread.
  void NotifyItemFinishedOnIOThread(importer::ImportItem import_item);

  // Notifies the importerhost that import has finished, and calls Release().
  void Cleanup();

  // Launches the task to start the external process.
  virtual void Start();

  // Creates a new UtilityProcessHost, which launches the import process.
  virtual void StartProcessOnIOThread(BrowserThread::ID thread_id);

  // Called by the ExternalProcessImporterHost on import cancel.
  virtual void Cancel();

  // UtilityProcessHost::Client implementation:
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers
  void OnImportStart();
  void OnImportFinished(bool succeeded, const std::string& error_msg);
  void OnImportItemStart(int item);
  void OnImportItemFinished(int item);
  void OnHistoryImportStart(size_t total_history_rows_count);
  void OnHistoryImportGroup(
      const std::vector<history::URLRow>& history_rows_group,
      int visit_source);
  void OnHomePageImportReady(const GURL& home_page);
  void OnBookmarksImportStart(const string16& first_folder_name,
                                      size_t total_bookmarks_count);
  void OnBookmarksImportGroup(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks_group);
  void OnFaviconsImportStart(size_t total_favicons_count);
  void OnFaviconsImportGroup(
      const std::vector<history::ImportedFaviconUsage>& favicons_group);
  void OnPasswordFormImportReady(const webkit_glue::PasswordForm& form);
  void OnKeywordsImportReady(
      const std::vector<TemplateURL>& template_urls,
      int default_keyword_index,
      bool unique_on_host_and_path);

 private:
  // These variables store data being collected from the importer until the
  // entire group has been collected and is ready to be written to the profile.
  std::vector<history::URLRow> history_rows_;
  std::vector<ProfileWriter::BookmarkEntry> bookmarks_;
  std::vector<history::ImportedFaviconUsage> favicons_;

  // Usually some variation on IDS_BOOKMARK_GROUP_...; the name of the folder
  // under which imported bookmarks will be placed.
  string16 bookmarks_first_folder_name_;

  // Total number of bookmarks to import.
  size_t total_bookmarks_count_;

  // Total number of history items to import.
  size_t total_history_rows_count_;

  // Total number of favicons to import.
  size_t total_favicons_count_;

  // Notifications received from the ProfileImportProcessHost are passed back
  // to process_importer_host_, which calls the ProfileWriter to record the
  // import data.  When the import process is done, process_importer_host_
  // deletes itself.
  ExternalProcessImporterHost* process_importer_host_;

  // Handles sending messages to the external process.  Deletes itself when
  // the external process dies (see ChildProcessHost::OnChildDied).
  UtilityProcessHost* utility_process_host_;

  // Data to be passed from the importer host to the external importer.
  const importer::SourceProfile& source_profile_;
  uint16 items_;

  // Takes import data coming over IPC and delivers it to be written by the
  // ProfileWriter.  Released by ExternalProcessImporterClient in its
  // destructor.
  InProcessImporterBridge* bridge_;

  // True if import process has been cancelled.
  bool cancelled_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterClient);
};

#endif  // CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_CLIENT_H_
