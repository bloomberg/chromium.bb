// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/profile_import_process_client.h"

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/profile_import_process_host.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "chrome/browser/search_engines/template_url.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/password_form.h"

ProfileImportProcessClient::ProfileImportProcessClient() {
}

void ProfileImportProcessClient::OnProcessCrashed(int exit_status) {
}

void ProfileImportProcessClient::OnImportStart() {
}

void ProfileImportProcessClient::OnImportFinished(
    bool succeeded,
    const std::string& error_msg) {
}

void ProfileImportProcessClient::OnImportItemStart(int item) {
}

void ProfileImportProcessClient::OnImportItemFinished(int item) {
}

void ProfileImportProcessClient::OnImportItemFailed(
    const std::string& error_msg) {
}

void ProfileImportProcessClient::OnHistoryImportStart(
    size_t total_history_rows_count) {
}

void ProfileImportProcessClient::OnHistoryImportGroup(
    const std::vector<history::URLRow>& history_rows_group,
    int visit_source) {
}

void ProfileImportProcessClient::OnHomePageImportReady(const GURL& home_page) {
}

void ProfileImportProcessClient::OnBookmarksImportStart(
    const string16& first_folder_name,
    int options,
    size_t total_bookmarks_count) {
}

void ProfileImportProcessClient::OnBookmarksImportGroup(
    const std::vector<ProfileWriter::BookmarkEntry>& bookmarks) {
}

void ProfileImportProcessClient::OnFaviconsImportStart(
    size_t total_favicons_count) {
}

void ProfileImportProcessClient::OnFaviconsImportGroup(
    const std::vector<history::ImportedFaviconUsage>& favicons_group) {
}

void ProfileImportProcessClient::OnPasswordFormImportReady(
    const webkit_glue::PasswordForm& form) {
}

void ProfileImportProcessClient::OnKeywordsImportReady(
    const std::vector<TemplateURL>& template_urls,
    int default_keyword_index,
    bool unique_on_host_and_path) {
}

bool ProfileImportProcessClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProfileImportProcessClient, message)
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
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

ProfileImportProcessClient::~ProfileImportProcessClient() {
}
