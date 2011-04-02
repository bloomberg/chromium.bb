// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_CLIENT_H_
#define CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_CLIENT_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/importer/profile_writer.h"

class GURL;
class TemplateURL;

namespace history {
class URLRow;
}

namespace IPC {
class Message;
}

namespace webkit_glue {
struct PasswordForm;
}

// An interface that must be implemented by consumers of the profile import
// process in order to get results back from the process host. The
// ProfileImportProcessHost calls the client's functions on the thread passed to
// it when it's created.
class ProfileImportProcessClient :
    public base::RefCountedThreadSafe<ProfileImportProcessClient> {
 public:
  ProfileImportProcessClient();

  // These methods are used by the ProfileImportProcessHost to pass messages
  // received from the external process back to the ImportProcessClient in
  // ImporterHost.
  virtual void OnProcessCrashed(int exit_status);
  virtual void OnImportStart();
  virtual void OnImportFinished(bool succeeded, const std::string& error_msg);
  virtual void OnImportItemStart(int item);
  virtual void OnImportItemFinished(int item);
  virtual void OnImportItemFailed(const std::string& error_msg);

  // These methods pass back data to be written to the user's profile from
  // the external process to the process host client.
  virtual void OnHistoryImportStart(size_t total_history_rows_count);
  virtual void OnHistoryImportGroup(
      const std::vector<history::URLRow>& history_rows_group,
      int visit_source); // visit_source has history::VisitSource type.

  virtual void OnHomePageImportReady(const GURL& home_page);

  virtual void OnBookmarksImportStart(
      const string16& first_folder_name,
      int options,
      size_t total_bookmarks_count);
  virtual void OnBookmarksImportGroup(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks);

  virtual void OnFaviconsImportStart(size_t total_favicons_count);
  virtual void OnFaviconsImportGroup(
      const std::vector<history::ImportedFaviconUsage>& favicons_group);

  virtual void OnPasswordFormImportReady(
      const webkit_glue::PasswordForm& form);

  virtual void OnKeywordsImportReady(
      const std::vector<TemplateURL>& template_urls,
      int default_keyword_index,
      bool unique_on_host_and_path);

  virtual bool OnMessageReceived(const IPC::Message& message);

 protected:
  friend class base::RefCountedThreadSafe<ProfileImportProcessClient>;

  virtual ~ProfileImportProcessClient();

 private:
  friend class ProfileImportProcessHost;

  DISALLOW_COPY_AND_ASSIGN(ProfileImportProcessClient);
};

#endif  // CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_CLIENT_H_
