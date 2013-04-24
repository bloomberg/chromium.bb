// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "content/public/common/password_form.h"
#include "ipc/ipc_sender.h"

#if defined(OS_WIN)
#include "components/webdata/encryptor/ie7_password.h"
#endif

namespace {
// Rather than sending all import items over IPC at once we chunk them into
// separate requests.  This avoids the case of a large import causing
// oversized IPC messages.
const int kNumBookmarksToSend = 100;
const int kNumHistoryRowsToSend = 100;
const int kNumFaviconsToSend = 100;
}

ExternalProcessImporterBridge::ExternalProcessImporterBridge(
    const DictionaryValue& localized_strings,
    IPC::Sender* sender,
    base::TaskRunner* task_runner)
    : sender_(sender),
      task_runner_(task_runner) {
  // Bridge needs to make its own copy because OS 10.6 autoreleases the
  // localized_strings value that is passed in (see http://crbug.com/46003 ).
  localized_strings_.reset(localized_strings.DeepCopy());
}

void ExternalProcessImporterBridge::AddBookmarks(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
    const string16& first_folder_name) {
  Send(new ProfileImportProcessHostMsg_NotifyBookmarksImportStart(
      first_folder_name, bookmarks.size()));

  std::vector<ProfileWriter::BookmarkEntry>::const_iterator it;
  for (it = bookmarks.begin(); it < bookmarks.end();
       it = it + kNumBookmarksToSend) {
    std::vector<ProfileWriter::BookmarkEntry> bookmark_group;
    std::vector<ProfileWriter::BookmarkEntry>::const_iterator end_group =
        it + kNumBookmarksToSend < bookmarks.end() ?
        it + kNumBookmarksToSend : bookmarks.end();
    bookmark_group.assign(it, end_group);

    Send(new ProfileImportProcessHostMsg_NotifyBookmarksImportGroup(
        bookmark_group));
  }
}

void ExternalProcessImporterBridge::AddHomePage(const GURL& home_page) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void ExternalProcessImporterBridge::AddIE7PasswordInfo(
    const IE7PasswordInfo& password_info) {
  NOTIMPLEMENTED();
}
#endif

void ExternalProcessImporterBridge::SetFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicons) {
  Send(new ProfileImportProcessHostMsg_NotifyFaviconsImportStart(
    favicons.size()));

  std::vector<history::ImportedFaviconUsage>::const_iterator it;
  for (it = favicons.begin(); it < favicons.end();
       it = it + kNumFaviconsToSend) {
    std::vector<history::ImportedFaviconUsage> favicons_group;
    std::vector<history::ImportedFaviconUsage>::const_iterator end_group =
        std::min(it + kNumFaviconsToSend, favicons.end());
    favicons_group.assign(it, end_group);

  Send(new ProfileImportProcessHostMsg_NotifyFaviconsImportGroup(
      favicons_group));
  }
}

void ExternalProcessImporterBridge::SetHistoryItems(
    const history::URLRows& rows,
    history::VisitSource visit_source) {
  Send(new ProfileImportProcessHostMsg_NotifyHistoryImportStart(rows.size()));

  history::URLRows::const_iterator it;
  for (it = rows.begin(); it < rows.end();
       it = it + kNumHistoryRowsToSend) {
    history::URLRows row_group;
    history::URLRows::const_iterator end_group =
        it + kNumHistoryRowsToSend < rows.end() ?
        it + kNumHistoryRowsToSend : rows.end();
    row_group.assign(it, end_group);

    Send(new ProfileImportProcessHostMsg_NotifyHistoryImportGroup(row_group,
         visit_source));
  }
}

void ExternalProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    bool unique_on_host_and_path) {
  Send(new ProfileImportProcessHostMsg_NotifyKeywordsReady(template_urls,
      unique_on_host_and_path));
  STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
}

void ExternalProcessImporterBridge::SetPasswordForm(
    const content::PasswordForm& form) {
  Send(new ProfileImportProcessHostMsg_NotifyPasswordFormReady(form));
}

void ExternalProcessImporterBridge::NotifyStarted() {
  Send(new ProfileImportProcessHostMsg_Import_Started());
}

void ExternalProcessImporterBridge::NotifyItemStarted(
    importer::ImportItem item) {
  Send(new ProfileImportProcessHostMsg_ImportItem_Started(item));
}

void ExternalProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  Send(new ProfileImportProcessHostMsg_ImportItem_Finished(item));
}

void ExternalProcessImporterBridge::NotifyEnded() {
  // The internal process detects import end when all items have been received.
}

string16 ExternalProcessImporterBridge::GetLocalizedString(int message_id) {
  string16 message;
  localized_strings_->GetString(base::IntToString(message_id), &message);
  return message;
}

ExternalProcessImporterBridge::~ExternalProcessImporterBridge() {}

void ExternalProcessImporterBridge::Send(IPC::Message* message) {
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ExternalProcessImporterBridge::SendInternal,
                 this, message));
}

void ExternalProcessImporterBridge::SendInternal(IPC::Message* message) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  sender_->Send(message);
}
