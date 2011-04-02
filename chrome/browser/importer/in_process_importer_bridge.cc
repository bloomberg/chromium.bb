// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/in_process_importer_bridge.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer_host.h"
#include "content/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/password_form.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/ie7_password.h"
#endif

InProcessImporterBridge::InProcessImporterBridge(ProfileWriter* writer,
                                                 ImporterHost* host)
    : writer_(writer),
      host_(host) {
}

void InProcessImporterBridge::AddBookmarkEntries(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
    const string16& first_folder_name,
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
    const IE7PasswordInfo& password_info) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(writer_, &ProfileWriter::AddIE7PasswordInfo,
      password_info));
}
#endif  // OS_WIN

void InProcessImporterBridge::SetFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicons) {
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
  LOG(ERROR) << "InProcessImporterBridge::SetPasswordForm";
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

string16 InProcessImporterBridge::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

InProcessImporterBridge::~InProcessImporterBridge() {}
