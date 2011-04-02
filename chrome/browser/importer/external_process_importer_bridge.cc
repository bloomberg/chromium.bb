// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_bridge.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/profile_import/profile_import_thread.h"
#include "webkit/glue/password_form.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
#endif

ExternalProcessImporterBridge::ExternalProcessImporterBridge(
    ProfileImportThread* profile_import_thread,
    const DictionaryValue& localized_strings)
    : profile_import_thread_(profile_import_thread) {
  // Bridge needs to make its own copy because OS 10.6 autoreleases the
  // localized_strings value that is passed in (see http://crbug.com/46003 ).
  localized_strings_.reset(localized_strings.DeepCopy());
}

void ExternalProcessImporterBridge::AddBookmarkEntries(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
    const string16& first_folder_name,
    int options) {
  profile_import_thread_->NotifyBookmarksImportReady(
      bookmarks, first_folder_name, options);
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
  profile_import_thread_->NotifyFaviconsImportReady(favicons);
}

void ExternalProcessImporterBridge::SetHistoryItems(
    const std::vector<history::URLRow>& rows,
    history::VisitSource visit_source) {
  profile_import_thread_->NotifyHistoryImportReady(rows, visit_source);
}

void ExternalProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
  profile_import_thread_->NotifyKeywordsReady(
      template_urls, default_keyword_index, unique_on_host_and_path);
}

void ExternalProcessImporterBridge::SetPasswordForm(
    const webkit_glue::PasswordForm& form) {
  profile_import_thread_->NotifyPasswordFormReady(form);
}

void ExternalProcessImporterBridge::NotifyStarted() {
  profile_import_thread_->NotifyStarted();
}

void ExternalProcessImporterBridge::NotifyItemStarted(
    importer::ImportItem item) {
  profile_import_thread_->NotifyItemStarted(item);
}

void ExternalProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  profile_import_thread_->NotifyItemEnded(item);
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
