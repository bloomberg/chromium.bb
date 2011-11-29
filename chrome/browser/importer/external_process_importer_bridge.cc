// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_bridge.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "webkit/glue/password_form.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
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
    IPC::Message::Sender* sender)
    : sender_(sender) {
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
    const std::vector<history::URLRow>& rows,
    history::VisitSource visit_source) {
  Send(new ProfileImportProcessHostMsg_NotifyHistoryImportStart(rows.size()));

  std::vector<history::URLRow>::const_iterator it;
  for (it = rows.begin(); it < rows.end();
       it = it + kNumHistoryRowsToSend) {
    std::vector<history::URLRow> row_group;
    std::vector<history::URLRow>::const_iterator end_group =
        it + kNumHistoryRowsToSend < rows.end() ?
        it + kNumHistoryRowsToSend : rows.end();
    row_group.assign(it, end_group);

    Send(new ProfileImportProcessHostMsg_NotifyHistoryImportGroup(row_group,
         visit_source));
  }
}

void ExternalProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
  std::vector<TemplateURL> urls;
  for (size_t i = 0; i < template_urls.size(); ++i) {
    urls.push_back(*template_urls[i]);
  }
  Send(new ProfileImportProcessHostMsg_NotifyKeywordsReady(urls,
      default_keyword_index, unique_on_host_and_path));
}

void ExternalProcessImporterBridge::SetPasswordForm(
    const webkit_glue::PasswordForm& form) {
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

bool ExternalProcessImporterBridge::Send(IPC::Message* message) {
  return sender_->Send(message);
}
