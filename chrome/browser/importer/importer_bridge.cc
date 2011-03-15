// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_bridge.h"

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_host.h"
#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
#endif
#include "chrome/browser/importer/importer_messages.h"
#include "chrome/profile_import/profile_import_thread.h"
#include "content/browser/browser_thread.h"
#include "content/common/child_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/password_form.h"

ImporterBridge::ImporterBridge() { }

ImporterBridge::~ImporterBridge() { }

InProcessImporterBridge::InProcessImporterBridge(ProfileWriter* writer,
                                                 ImporterHost* host)
    : writer_(writer), host_(host) {
}

void InProcessImporterBridge::AddBookmarkEntries(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
    const std::wstring& first_folder_name,
    int options) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          writer_, &ProfileWriter::AddBookmarkEntry, bookmarks,
          first_folder_name, options));
}

void InProcessImporterBridge::AddHomePage(const GURL &home_page) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddHomepage, home_page));
}

#if defined(OS_WIN)
void InProcessImporterBridge::AddIE7PasswordInfo(
    const IE7PasswordInfo password_info) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddIE7PasswordInfo,
      password_info));
}
#endif  // OS_WIN

void InProcessImporterBridge::SetFavicons(
    const std::vector<history::ImportedFavIconUsage>& favicons) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddFavicons, favicons));
}

void InProcessImporterBridge::SetHistoryItems(
    const std::vector<history::URLRow> &rows,
    history::VisitSource visit_source) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddHistoryPage,
                        rows, visit_source));
}

void InProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          writer_, &ProfileWriter::AddKeywords, template_urls,
          default_keyword_index, unique_on_host_and_path));
}

void InProcessImporterBridge::SetPasswordForm(
    const webkit_glue::PasswordForm& form) {
  LOG(ERROR) << "IPImporterBridge::SetPasswordForm";
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddPasswordForm, form));
}

void InProcessImporterBridge::NotifyStarted() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(host_, &ImporterHost::NotifyImportStarted));
}

void InProcessImporterBridge::NotifyItemStarted(importer::ImportItem item) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(host_, &ImporterHost::NotifyImportItemStarted, item));
}

void InProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(host_, &ImporterHost::NotifyImportItemEnded, item));
}

void InProcessImporterBridge::NotifyEnded() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(host_, &ImporterHost::NotifyImportEnded));
}

std::wstring InProcessImporterBridge::GetLocalizedString(int message_id) {
  return UTF16ToWideHack(l10n_util::GetStringUTF16(message_id));
}

InProcessImporterBridge::~InProcessImporterBridge() {}

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
    const std::wstring& first_folder_name, int options) {
  profile_import_thread_->NotifyBookmarksImportReady(bookmarks,
      first_folder_name, options);
}

void ExternalProcessImporterBridge::AddHomePage(const GURL &home_page) {
  // TODO(mirandac): remove home page import from code base.
  // http://crbug.com/45678 :-)
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void ExternalProcessImporterBridge::AddIE7PasswordInfo(
    const IE7PasswordInfo password_info) {
  NOTIMPLEMENTED();
}
#endif

void ExternalProcessImporterBridge::SetFavicons(
    const std::vector<history::ImportedFavIconUsage>& favicons) {
  profile_import_thread_->NotifyFavIconsImportReady(favicons);
}

void ExternalProcessImporterBridge::SetHistoryItems(
    const std::vector<history::URLRow> &rows,
    history::VisitSource visit_source) {
  profile_import_thread_->NotifyHistoryImportReady(rows, visit_source);
}

void ExternalProcessImporterBridge::SetKeywords(
    const std::vector<TemplateURL*>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
  profile_import_thread_->NotifyKeywordsReady(template_urls,
      default_keyword_index, unique_on_host_and_path);
}

void ExternalProcessImporterBridge::SetPasswordForm(
    const webkit_glue::PasswordForm& form) {
  profile_import_thread_->NotifyPasswordFormReady(form);
}

void ExternalProcessImporterBridge::NotifyItemStarted(
    importer::ImportItem item) {
  profile_import_thread_->NotifyItemStarted(item);
}

void ExternalProcessImporterBridge::NotifyItemEnded(importer::ImportItem item) {
  profile_import_thread_->NotifyItemEnded(item);
}

void ExternalProcessImporterBridge::NotifyStarted() {
  profile_import_thread_->NotifyStarted();
}

void ExternalProcessImporterBridge::NotifyEnded() {
  // The internal process detects import end when all items have been received.
}

// TODO(viettrungluu): convert to string16.
std::wstring ExternalProcessImporterBridge::GetLocalizedString(
    int message_id) {
  string16 message;
  localized_strings_->GetString(base::IntToString(message_id), &message);
  return UTF16ToWideHack(message);
}

ExternalProcessImporterBridge::~ExternalProcessImporterBridge() {}
