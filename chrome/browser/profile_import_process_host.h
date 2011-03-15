// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_IMPORT_PROCESS_HOST_H_
#define CHROME_BROWSER_PROFILE_IMPORT_PROCESS_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"

namespace webkit_glue {
struct PasswordForm;
}

// Browser-side host to a profile import process.  This class lives only on
// the IO thread.  It passes messages back to the |thread_id_| thread through
// a client object.
class ProfileImportProcessHost : public BrowserChildProcessHost {
 public:

  // An interface that must be implemented by consumers of the profile import
  // process in order to get results back from the process host.  The
  // ProfileImportProcessHost calls the client's functions on the thread passed
  // to it when it's created.
  class ImportProcessClient :
      public base::RefCountedThreadSafe<ImportProcessClient> {
   public:
    ImportProcessClient();

    // These methods are used by the ProfileImportProcessHost to pass messages
    // received from the external process back to the ImportProcessClient in
    // ImporterHost.
    virtual void OnProcessCrashed(int exit_status) {}
    virtual void OnImportStart() {}
    virtual void OnImportFinished(bool succeeded, std::string error_msg) {}
    virtual void OnImportItemStart(int item) {}
    virtual void OnImportItemFinished(int item) {}
    virtual void OnImportItemFailed(std::string error_msg) {}

    // These methods pass back data to be written to the user's profile from
    // the external process to the process host client.
    virtual void OnHistoryImportStart(size_t total_history_rows_count) {}
    virtual void OnHistoryImportGroup(
        const std::vector<history::URLRow> &history_rows_group,
        int visit_source) {}  // visit_source has history::VisitSource type.

    virtual void OnHomePageImportReady(
        const GURL& home_page) {}

    virtual void OnBookmarksImportStart(
        const std::wstring first_folder_name,
        int options, size_t total_bookmarks_count) {}
    virtual void OnBookmarksImportGroup(
        const std::vector<ProfileWriter::BookmarkEntry>& bookmarks) {}

    virtual void OnFavIconsImportStart(size_t total_favicons_count) {}
    virtual void OnFavIconsImportGroup(
        const std::vector<history::ImportedFavIconUsage>& favicons_group) {}

    virtual void OnPasswordFormImportReady(
        const webkit_glue::PasswordForm& form) {}

    virtual void OnKeywordsImportReady(
        const std::vector<TemplateURL>& template_urls,
            int default_keyword_index, bool unique_on_host_and_path) {}

    virtual bool OnMessageReceived(const IPC::Message& message);

   protected:
    friend class base::RefCountedThreadSafe<ImportProcessClient>;

    virtual ~ImportProcessClient();

   private:
    friend class ProfileImportProcessHost;

    DISALLOW_COPY_AND_ASSIGN(ImportProcessClient);
  };

  // |resource_dispatcher| is used in the base BrowserChildProcessHost class to
  // manage IPC requests.
  // |import_process_client| implements callbacks which are triggered by
  // incoming IPC messages.  This client creates an interface between IPC
  // messages received by the ProfileImportProcessHost and the internal
  // importer_bridge.
  // |thread_id| gives the thread where the client lives. The
  // ProfileImportProcessHost spawns tasks on this thread for the client.
  ProfileImportProcessHost(ResourceDispatcherHost* resource_dispatcher,
                           ImportProcessClient* import_process_client,
                           BrowserThread::ID thread_id);
  virtual ~ProfileImportProcessHost();

  // |profile_info|, |items|, and |import_to_bookmark_bar| are all needed by
  // the external importer process.
  bool StartProfileImportProcess(const importer::ProfileInfo& profile_info,
                                 int items, bool import_to_bookmark_bar);

  // Cancel the external import process.
  bool CancelProfileImportProcess();

  // Report that an item has been successfully imported.  We need to make
  // sure that all import messages have come across the wire before the
  // external import process shuts itself down.
  bool ReportImportItemFinished(importer::ImportItem item);

 protected:
  // Allow these methods to be overridden for tests.
  virtual FilePath GetProfileImportProcessCmd();

 private:
  // Launch the new process.
  bool StartProcess();

  // Called by the external importer process to send messages back to the
  // ImportProcessClient.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Overridden from BrowserChildProcessHost:
  virtual void OnProcessCrashed(int exit_code);
  virtual bool CanShutdown();

  // Receives messages to be passed back to the importer host.
  scoped_refptr<ImportProcessClient> import_process_client_;

  // The thread where the import_process_client_ lives.
  BrowserThread::ID thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImportProcessHost);
};

#endif  // CHROME_BROWSER_PROFILE_IMPORT_PROCESS_HOST_H_
