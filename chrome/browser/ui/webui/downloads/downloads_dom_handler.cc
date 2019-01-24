// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"

#include <algorithm>
#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace {

enum DownloadsDOMEvent {
  DOWNLOADS_DOM_EVENT_GET_DOWNLOADS = 0,
  DOWNLOADS_DOM_EVENT_OPEN_FILE = 1,
  DOWNLOADS_DOM_EVENT_DRAG = 2,
  DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS = 3,
  DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS = 4,
  DOWNLOADS_DOM_EVENT_SHOW = 5,
  DOWNLOADS_DOM_EVENT_PAUSE = 6,
  DOWNLOADS_DOM_EVENT_REMOVE = 7,
  DOWNLOADS_DOM_EVENT_CANCEL = 8,
  DOWNLOADS_DOM_EVENT_CLEAR_ALL = 9,
  DOWNLOADS_DOM_EVENT_OPEN_FOLDER = 10,
  DOWNLOADS_DOM_EVENT_RESUME = 11,
  DOWNLOADS_DOM_EVENT_RETRY_DOWNLOAD = 12,
  DOWNLOADS_DOM_EVENT_MAX
};

void CountDownloadsDOMEvents(DownloadsDOMEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.DOMEvent",
                            event,
                            DOWNLOADS_DOM_EVENT_MAX);
}

}  // namespace

MdDownloadsDOMHandler::MdDownloadsDOMHandler(
    md_downloads::mojom::PageHandlerRequest request,
    md_downloads::mojom::PagePtr page,
    content::DownloadManager* download_manager,
    content::WebUI* web_ui)
    : list_tracker_(download_manager, std::move(page)),
      web_ui_(web_ui),
      binding_(this, std::move(request)) {
  // Create our fileicon data source.
  content::URLDataSource::Add(
      Profile::FromBrowserContext(download_manager->GetBrowserContext()),
      std::make_unique<FileIconSource>());
  CheckForRemovedFiles();
}

MdDownloadsDOMHandler::~MdDownloadsDOMHandler() {
  list_tracker_.Stop();
  list_tracker_.Reset();
  if (!render_process_gone_)
    CheckForRemovedFiles();
  FinalizeRemovals();
}

void MdDownloadsDOMHandler::RenderProcessGone(base::TerminationStatus status) {
  // TODO(dbeam): WebUI + WebUIMessageHandler should do this automatically.
  // http://crbug.com/610450
  render_process_gone_ = true;
}

void MdDownloadsDOMHandler::GetDownloads(
    const std::vector<std::string>& search_terms) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_GET_DOWNLOADS);

  bool terms_changed = list_tracker_.SetSearchTerms(search_terms);
  if (terms_changed)
    list_tracker_.Reset();

  list_tracker_.StartAndSendChunk();
}

void MdDownloadsDOMHandler::OpenFileRequiringGesture(const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteractiveInputEvent()) {
    LOG(ERROR) << "OpenFileRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FILE);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    file->OpenDownload();
}

void MdDownloadsDOMHandler::Drag(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DRAG);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file)
    return;

  content::WebContents* web_contents = GetWebUIWebContents();
  // |web_contents| is only NULL in the test.
  if (!web_contents)
    return;

  if (file->GetState() != download::DownloadItem::COMPLETE)
    return;

  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      file->GetTargetFilePath(), IconLoader::NORMAL);
  gfx::NativeView view = web_contents->GetNativeView();
  {
    // Enable nested tasks during DnD, while |DragDownload()| blocks.
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
    DragDownloadItem(file, icon, view);
  }
}

void MdDownloadsDOMHandler::SaveDangerousRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteractiveInputEvent()) {
    LOG(ERROR) << "SaveDangerousRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    ShowDangerPrompt(file);
}

void MdDownloadsDOMHandler::DiscardDangerous(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS);
  RemoveDownloadInArgs(id);
}

void MdDownloadsDOMHandler::RetryDownload(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_RETRY_DOWNLOAD);

  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file)
    return;
  content::WebContents* web_contents = GetWebUIWebContents();
  content::RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  const GURL url = file->GetURL();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("downloads_dom_handler", R"(
        semantics {
          sender: "The downloads page."
          description: "Retrying a download."
          trigger:
            "The user selects the 'Retry' button for a cancelled download on "
            "the downloads page."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings, but it's only "
            "triggered by user request."
          policy_exception_justification: "Not implemented."
        })");

  auto dl_params = std::make_unique<download::DownloadUrlParameters>(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRenderViewHost()->GetRoutingID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
  dl_params->set_content_initiated(true);
  dl_params->set_initiator(url::Origin::Create(GURL("chrome://downloads")));
  dl_params->set_download_source(download::DownloadSource::RETRY);

  content::BrowserContext::GetDownloadManager(web_contents->GetBrowserContext())
      ->DownloadUrl(std::move(dl_params));
}

void MdDownloadsDOMHandler::Show(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SHOW);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    file->ShowDownloadInShell();
}

void MdDownloadsDOMHandler::Pause(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_PAUSE);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    file->Pause();
}

void MdDownloadsDOMHandler::Resume(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_RESUME);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    file->Resume(true);
}

void MdDownloadsDOMHandler::Remove(const std::string& id) {
  if (!IsDeletingHistoryAllowed())
    return;

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REMOVE);
  RemoveDownloadInArgs(id);
}

void MdDownloadsDOMHandler::Undo() {
  // TODO(dbeam): handle more than removed downloads someday?
  if (removals_.empty())
    return;

  const IdSet last_removed_ids = removals_.back();
  removals_.pop_back();

  const bool undoing_clear_all = last_removed_ids.size() > 1;
  if (undoing_clear_all) {
    list_tracker_.Reset();
    list_tracker_.Stop();
  }

  for (auto id : last_removed_ids) {
    download::DownloadItem* download = GetDownloadById(id);
    if (!download)
      continue;

    DownloadItemModel model(download);
    model.SetShouldShowInShelf(true);
    model.SetIsBeingRevived(true);

    download->UpdateObservers();

    model.SetIsBeingRevived(false);
  }

  if (undoing_clear_all)
    list_tracker_.StartAndSendChunk();
}

void MdDownloadsDOMHandler::Cancel(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file)
    file->Cancel(true);
}

void MdDownloadsDOMHandler::ClearAll() {
  if (!IsDeletingHistoryAllowed()) {
    // This should only be reached during tests.
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CLEAR_ALL);

  list_tracker_.Reset();
  list_tracker_.Stop();

  DownloadVector downloads;
  if (GetMainNotifierManager())
    GetMainNotifierManager()->GetAllDownloads(&downloads);
  if (GetOriginalNotifierManager())
    GetOriginalNotifierManager()->GetAllDownloads(&downloads);
  RemoveDownloads(downloads);

  list_tracker_.StartAndSendChunk();
}

void MdDownloadsDOMHandler::RemoveDownloads(const DownloadVector& to_remove) {
  IdSet ids;

  for (auto* download : to_remove) {
    if (download->IsDangerous()) {
      // Don't allow users to revive dangerous downloads; just nuke 'em.
      download->Remove();
      continue;
    }

    DownloadItemModel item_model(download);
    if (!item_model.ShouldShowInShelf() ||
        download->GetState() == download::DownloadItem::IN_PROGRESS) {
      continue;
    }

    item_model.SetShouldShowInShelf(false);
    ids.insert(download->GetId());
    download->UpdateObservers();
  }

  if (!ids.empty())
    removals_.push_back(ids);
}

void MdDownloadsDOMHandler::OpenDownloadsFolderRequiringGesture() {
  if (!GetWebUIWebContents()->HasRecentInteractiveInputEvent()) {
    LOG(ERROR) << "OpenDownloadsFolderRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FOLDER);
  content::DownloadManager* manager = GetMainNotifierManager();
  if (manager) {
    platform_util::OpenItem(
        Profile::FromBrowserContext(manager->GetBrowserContext()),
        DownloadPrefs::FromDownloadManager(manager)->DownloadPath(),
        platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());
  }
}

// MdDownloadsDOMHandler, private: --------------------------------------------

content::DownloadManager* MdDownloadsDOMHandler::GetMainNotifierManager()
    const {
  return list_tracker_.GetMainNotifierManager();
}

content::DownloadManager* MdDownloadsDOMHandler::GetOriginalNotifierManager()
    const {
  return list_tracker_.GetOriginalNotifierManager();
}

void MdDownloadsDOMHandler::FinalizeRemovals() {
  while (!removals_.empty()) {
    const IdSet remove = removals_.back();
    removals_.pop_back();

    for (const auto id : remove) {
      download::DownloadItem* download = GetDownloadById(id);
      if (download)
        download->Remove();
    }
  }
}

void MdDownloadsDOMHandler::ShowDangerPrompt(
    download::DownloadItem* dangerous_item) {
  DownloadDangerPrompt* danger_prompt = DownloadDangerPrompt::Create(
      dangerous_item,
      GetWebUIWebContents(),
      false,
      base::Bind(&MdDownloadsDOMHandler::DangerPromptDone,
                 weak_ptr_factory_.GetWeakPtr(), dangerous_item->GetId()));
  // danger_prompt will delete itself.
  DCHECK(danger_prompt);
}

void MdDownloadsDOMHandler::DangerPromptDone(
    int download_id, DownloadDangerPrompt::Action action) {
  if (action != DownloadDangerPrompt::ACCEPT)
    return;
  download::DownloadItem* item = NULL;
  if (GetMainNotifierManager())
    item = GetMainNotifierManager()->GetDownload(download_id);
  if (!item && GetOriginalNotifierManager())
    item = GetOriginalNotifierManager()->GetDownload(download_id);
  if (!item || item->IsDone())
    return;
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  item->ValidateDangerousDownload();
}

bool MdDownloadsDOMHandler::IsDeletingHistoryAllowed() {
  content::DownloadManager* manager = GetMainNotifierManager();
  return manager &&
         Profile::FromBrowserContext(manager->GetBrowserContext())->
             GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

download::DownloadItem* MdDownloadsDOMHandler::GetDownloadByStringId(
    const std::string& id) {
  uint64_t id_num;
  if (!base::StringToUint64(id, &id_num)) {
    NOTREACHED();
    return nullptr;
  }

  return GetDownloadById(static_cast<uint32_t>(id_num));
}

download::DownloadItem* MdDownloadsDOMHandler::GetDownloadById(uint32_t id) {
  download::DownloadItem* item = NULL;
  if (GetMainNotifierManager())
    item = GetMainNotifierManager()->GetDownload(id);
  if (!item && GetOriginalNotifierManager())
    item = GetOriginalNotifierManager()->GetDownload(id);
  return item;
}

content::WebContents* MdDownloadsDOMHandler::GetWebUIWebContents() {
  return web_ui_->GetWebContents();
}

void MdDownloadsDOMHandler::CheckForRemovedFiles() {
  if (GetMainNotifierManager())
    GetMainNotifierManager()->CheckForHistoryFilesRemoval();
  if (GetOriginalNotifierManager())
    GetOriginalNotifierManager()->CheckForHistoryFilesRemoval();
}

void MdDownloadsDOMHandler::RemoveDownloadInArgs(const std::string& id) {
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file)
    return;

  DownloadVector downloads;
  downloads.push_back(file);
  RemoveDownloads(downloads);
}
