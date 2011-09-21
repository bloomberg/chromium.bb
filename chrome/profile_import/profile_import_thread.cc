// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profile_import/profile_import_thread.h"

#include <stddef.h>
#include <algorithm>

#include "base/location.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/importer/external_process_importer_bridge.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_type.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "chrome/browser/search_engines/template_url.h"
#include "content/common/child_process.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

namespace {
// Rather than sending all import items over IPC at once we chunk them into
// separate requests.  This avoids the case of a large import causing
// oversized IPC messages.
const int kNumBookmarksToSend = 100;
const int kNumHistoryRowsToSend = 100;
const int kNumFaviconsToSend = 100;
}

ProfileImportThread::ProfileImportThread()
    : bridge_(NULL),
      items_to_import_(0),
      importer_(NULL) {
  ChildProcess::current()->AddRefProcess();  // Balanced in Cleanup().
}

ProfileImportThread::~ProfileImportThread() {}

bool ProfileImportThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProfileImportThread, msg)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_StartImport,
                        OnImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_CancelImport,
                        OnImportCancel)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_ReportImportItemFinished,
                        OnImportItemFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ProfileImportThread::OnImportStart(
    const importer::SourceProfile& source_profile,
    uint16 items,
    const DictionaryValue& localized_strings) {
  bridge_ = new ExternalProcessImporterBridge(this, localized_strings);
  importer_ = importer::CreateImporterByType(source_profile.importer_type);
  if (!importer_) {
    Send(new ProfileImportProcessHostMsg_Import_Finished(false,
        "Importer could not be created."));
    return;
  }

  items_to_import_ = items;

  // Create worker thread in which importer runs.
  import_thread_.reset(new base::Thread("import_thread"));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!import_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Cleanup();
  }
  import_thread_->message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(importer_.get(),
                                   &Importer::StartImport,
                                   source_profile,
                                   items,
                                   bridge_));
}

void ProfileImportThread::OnImportCancel() {
  Cleanup();
}

void ProfileImportThread::OnImportItemFinished(uint16 item) {
  items_to_import_ ^= item;  // Remove finished item from mask.
  // If we've finished with all items, notify the browser process.
  if (items_to_import_ == 0)
    NotifyEnded();
}

void ProfileImportThread::NotifyStarted() {
  Send(new ProfileImportProcessHostMsg_Import_Started());
}

void ProfileImportThread::NotifyItemStarted(importer::ImportItem item) {
  Send(new ProfileImportProcessHostMsg_ImportItem_Started(item));
}

void ProfileImportThread::NotifyItemEnded(importer::ImportItem item) {
  Send(new ProfileImportProcessHostMsg_ImportItem_Finished(item));
}

void ProfileImportThread::NotifyEnded() {
  Send(new ProfileImportProcessHostMsg_Import_Finished(true, ""));
  Cleanup();
}

void ProfileImportThread::NotifyHistoryImportReady(
    const std::vector<history::URLRow> &rows,
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

void ProfileImportThread::NotifyHomePageImportReady(
    const GURL& home_page) {
  NOTIMPLEMENTED();
}

void ProfileImportThread::NotifyBookmarksImportReady(
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

void ProfileImportThread::NotifyFaviconsImportReady(
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

void ProfileImportThread::NotifyPasswordFormReady(
    const webkit_glue::PasswordForm& form) {
  Send(new ProfileImportProcessHostMsg_NotifyPasswordFormReady(form));
}

void ProfileImportThread::NotifyKeywordsReady(
    const std::vector<TemplateURL*>& template_urls,
        int default_keyword_index, bool unique_on_host_and_path) {
  std::vector<TemplateURL> urls;
  for (size_t i = 0; i < template_urls.size(); ++i) {
    urls.push_back(*template_urls[i]);
  }
  Send(new ProfileImportProcessHostMsg_NotifyKeywordsReady(urls,
      default_keyword_index, unique_on_host_and_path));
}

void ProfileImportThread::Cleanup() {
  importer_->Cancel();
  importer_ = NULL;
  bridge_ = NULL;
  ChildProcess::current()->ReleaseProcess();
}
