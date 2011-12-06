// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_client.h"

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/in_process_importer_bridge.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

ExternalProcessImporterClient::ExternalProcessImporterClient(
    ExternalProcessImporterHost* importer_host,
    const importer::SourceProfile& source_profile,
    uint16 items,
    InProcessImporterBridge* bridge)
    : total_bookmarks_count_(0),
      total_history_rows_count_(0),
      total_favicons_count_(0),
      process_importer_host_(importer_host),
      utility_process_host_(NULL),
      source_profile_(source_profile),
      items_(items),
      bridge_(bridge),
      cancelled_(false) {
  bridge_->AddRef();
  process_importer_host_->NotifyImportStarted();
}

ExternalProcessImporterClient::~ExternalProcessImporterClient() {
  bridge_->Release();
}

void ExternalProcessImporterClient::CancelImportProcessOnIOThread() {
  utility_process_host_->Send(new ProfileImportProcessMsg_CancelImport());
}

void ExternalProcessImporterClient::NotifyItemFinishedOnIOThread(
    importer::ImportItem import_item) {
  utility_process_host_->Send(
      new ProfileImportProcessMsg_ReportImportItemFinished(import_item));
}

void ExternalProcessImporterClient::Cleanup() {
  if (cancelled_)
    return;

  if (process_importer_host_)
    process_importer_host_->NotifyImportEnded();
  Release();
}

void ExternalProcessImporterClient::Start() {
  AddRef();  // balanced in Cleanup.
  BrowserThread::ID thread_id;
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExternalProcessImporterClient::StartProcessOnIOThread,
                 this,
                 thread_id));
}

void ExternalProcessImporterClient::StartProcessOnIOThread(
    BrowserThread::ID thread_id) {
  utility_process_host_ = new UtilityProcessHost(this, thread_id);
  utility_process_host_->set_no_sandbox(true);

#if defined(OS_MACOSX)
  base::environment_vector env;
  std::string dylib_path = GetFirefoxDylibPath().value();
  if (!dylib_path.empty())
    env.push_back(std::make_pair("DYLD_FALLBACK_LIBRARY_PATH", dylib_path));
  utility_process_host_->set_env(env);
#endif

  // Dictionary of all localized strings that could be needed by the importer
  // in the external process.
  DictionaryValue localized_strings;
  localized_strings.SetString(
      base::IntToString(IDS_BOOKMARK_GROUP_FROM_FIREFOX),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_GROUP_FROM_FIREFOX));
  localized_strings.SetString(
      base::IntToString(IDS_BOOKMARK_GROUP_FROM_SAFARI),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_GROUP_FROM_SAFARI));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_FIREFOX),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_FIREFOX));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_GOOGLE_TOOLBAR),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_GOOGLE_TOOLBAR));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_SAFARI),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_SAFARI));
  localized_strings.SetString(
      base::IntToString(IDS_BOOKMARK_BAR_FOLDER_NAME),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BAR_FOLDER_NAME));

  utility_process_host_->Send(new ProfileImportProcessMsg_StartImport(
      source_profile_, items_, localized_strings));
}

void ExternalProcessImporterClient::Cancel() {
  if (cancelled_)
    return;

  cancelled_ = true;
  if (utility_process_host_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &ExternalProcessImporterClient::CancelImportProcessOnIOThread,
            this));
  }
  Release();
}

void ExternalProcessImporterClient::OnProcessCrashed(int exit_code) {
  utility_process_host_ = NULL;
  if (cancelled_)
    return;

  process_importer_host_->Cancel();
}

bool ExternalProcessImporterClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExternalProcessImporterClient, message)
    // Notification messages about the state of the import process.
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_Import_Started,
                        OnImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_Import_Finished,
                        OnImportFinished)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_ImportItem_Started,
                        OnImportItemStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_ImportItem_Finished,
                        OnImportItemFinished)
    // Data messages containing items to be written to the user profile.
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHistoryImportStart,
                        OnHistoryImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHistoryImportGroup,
                        OnHistoryImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHomePageImportReady,
                        OnHomePageImportReady)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyBookmarksImportStart,
                        OnBookmarksImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyBookmarksImportGroup,
                        OnBookmarksImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyFaviconsImportStart,
                        OnFaviconsImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyFaviconsImportGroup,
                        OnFaviconsImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyPasswordFormReady,
                        OnPasswordFormImportReady)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyKeywordsReady,
                        OnKeywordsImportReady)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExternalProcessImporterClient::OnImportStart() {
  if (cancelled_)
    return;

  bridge_->NotifyStarted();
}

void ExternalProcessImporterClient::OnImportFinished(
    bool succeeded, const std::string& error_msg) {
  if (cancelled_)
    return;

  if (!succeeded)
    LOG(WARNING) << "Import failed.  Error: " << error_msg;
  Cleanup();
}

void ExternalProcessImporterClient::OnImportItemStart(int item_data) {
  if (cancelled_)
    return;

  bridge_->NotifyItemStarted(static_cast<importer::ImportItem>(item_data));
}

void ExternalProcessImporterClient::OnImportItemFinished(int item_data) {
  if (cancelled_)
    return;

  importer::ImportItem import_item =
      static_cast<importer::ImportItem>(item_data);
  bridge_->NotifyItemEnded(import_item);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ExternalProcessImporterClient::NotifyItemFinishedOnIOThread,
                 this,
                 import_item));
}

void ExternalProcessImporterClient::OnHistoryImportStart(
    size_t total_history_rows_count) {
  if (cancelled_)
    return;

  total_history_rows_count_ = total_history_rows_count;
  history_rows_.reserve(total_history_rows_count);
}

void ExternalProcessImporterClient::OnHistoryImportGroup(
    const std::vector<history::URLRow>& history_rows_group,
    int visit_source) {
  if (cancelled_)
    return;

  history_rows_.insert(history_rows_.end(), history_rows_group.begin(),
                       history_rows_group.end());
  if (history_rows_.size() == total_history_rows_count_)
    bridge_->SetHistoryItems(history_rows_,
                             static_cast<history::VisitSource>(visit_source));
}

void ExternalProcessImporterClient::OnHomePageImportReady(
    const GURL& home_page) {
  if (cancelled_)
    return;

  bridge_->AddHomePage(home_page);
}

void ExternalProcessImporterClient::OnBookmarksImportStart(
    const string16& first_folder_name,
    size_t total_bookmarks_count) {
  if (cancelled_)
    return;

  bookmarks_first_folder_name_ = first_folder_name;
  total_bookmarks_count_ = total_bookmarks_count;
  bookmarks_.reserve(total_bookmarks_count);
}

void ExternalProcessImporterClient::OnBookmarksImportGroup(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks_group) {
  if (cancelled_)
    return;

  // Collect sets of bookmarks from importer process until we have reached
  // total_bookmarks_count_:
  bookmarks_.insert(bookmarks_.end(), bookmarks_group.begin(),
                    bookmarks_group.end());
  if (bookmarks_.size() == total_bookmarks_count_)
    bridge_->AddBookmarks(bookmarks_, bookmarks_first_folder_name_);
}

void ExternalProcessImporterClient::OnFaviconsImportStart(
    size_t total_favicons_count) {
  if (cancelled_)
    return;

  total_favicons_count_ = total_favicons_count;
  favicons_.reserve(total_favicons_count);
}

void ExternalProcessImporterClient::OnFaviconsImportGroup(
    const std::vector<history::ImportedFaviconUsage>& favicons_group) {
  if (cancelled_)
    return;

  favicons_.insert(favicons_.end(), favicons_group.begin(),
                    favicons_group.end());
  if (favicons_.size() == total_favicons_count_)
    bridge_->SetFavicons(favicons_);
}

void ExternalProcessImporterClient::OnPasswordFormImportReady(
    const webkit_glue::PasswordForm& form) {
  if (cancelled_)
    return;

  bridge_->SetPasswordForm(form);
}

void ExternalProcessImporterClient::OnKeywordsImportReady(
    const std::vector<TemplateURL>& template_urls,
        int default_keyword_index, bool unique_on_host_and_path) {
  if (cancelled_)
    return;

  std::vector<TemplateURL*> template_url_vec;
  template_url_vec.reserve(template_urls.size());
  std::vector<TemplateURL>::const_iterator iter;
  for (iter = template_urls.begin();
       iter != template_urls.end();
       ++iter) {
    template_url_vec.push_back(new TemplateURL(*iter));
  }
  bridge_->SetKeywords(template_url_vec, default_keyword_index,
                       unique_on_host_and_path);
}
