// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/downloads_dom_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/fileicon_source.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

// Maximum number of downloads to show. TODO(glen): Remove this and instead
// stuff the downloads down the pipe slowly.
static const int kMaxDownloads = 150;

// Sort DownloadItems into descending order by their start time.
class DownloadItemSorter : public std::binary_function<DownloadItem*,
                                                       DownloadItem*,
                                                       bool> {
 public:
  bool operator()(const DownloadItem* lhs, const DownloadItem* rhs) {
    return lhs->start_time() > rhs->start_time();
  }
};

} // namespace

DownloadsDOMHandler::DownloadsDOMHandler(DownloadManager* dlm)
    : search_text_(),
      download_manager_(dlm) {
  // Create our fileicon data source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(new FileIconSource())));
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
}

// DownloadsDOMHandler, public: -----------------------------------------------

void DownloadsDOMHandler::Init() {
  download_manager_->AddObserver(this);
}

void DownloadsDOMHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getDownloads",
      NewCallback(this, &DownloadsDOMHandler::HandleGetDownloads));
  dom_ui_->RegisterMessageCallback("openFile",
      NewCallback(this, &DownloadsDOMHandler::HandleOpenFile));

  dom_ui_->RegisterMessageCallback("drag",
      NewCallback(this, &DownloadsDOMHandler::HandleDrag));

  dom_ui_->RegisterMessageCallback("saveDangerous",
      NewCallback(this, &DownloadsDOMHandler::HandleSaveDangerous));
  dom_ui_->RegisterMessageCallback("discardDangerous",
      NewCallback(this, &DownloadsDOMHandler::HandleDiscardDangerous));
  dom_ui_->RegisterMessageCallback("show",
      NewCallback(this, &DownloadsDOMHandler::HandleShow));
  dom_ui_->RegisterMessageCallback("togglepause",
      NewCallback(this, &DownloadsDOMHandler::HandlePause));
  dom_ui_->RegisterMessageCallback("resume",
      NewCallback(this, &DownloadsDOMHandler::HandlePause));
  dom_ui_->RegisterMessageCallback("remove",
      NewCallback(this, &DownloadsDOMHandler::HandleRemove));
  dom_ui_->RegisterMessageCallback("cancel",
      NewCallback(this, &DownloadsDOMHandler::HandleCancel));
  dom_ui_->RegisterMessageCallback("clearAll",
      NewCallback(this, &DownloadsDOMHandler::HandleClearAll));
}

void DownloadsDOMHandler::OnDownloadUpdated(DownloadItem* download) {
  // Get the id for the download. Our downloads are sorted latest to first,
  // and the id is the index into that list. We should be careful of sync
  // errors between the UI and the download_items_ list (we may wish to use
  // something other than 'id').
  OrderedDownloads::iterator it = find(download_items_.begin(),
                                       download_items_.end(),
                                       download);
  if (it == download_items_.end())
    return;
  const int id = static_cast<int>(it - download_items_.begin());

  ListValue results_value;
  results_value.Append(download_util::CreateDownloadItemValue(download, id));
  dom_ui_->CallJavascriptFunction(L"downloadUpdated", results_value);
}

// A download has started or been deleted. Query our DownloadManager for the
// current set of downloads, which will call us back in SetDownloads once it
// has retrieved them.
void DownloadsDOMHandler::ModelChanged() {
  ClearDownloadItems();
  download_manager_->GetDownloads(this, search_text_);
}

void DownloadsDOMHandler::SetDownloads(
    std::vector<DownloadItem*>& downloads) {
  ClearDownloadItems();

  // Swap new downloads in.
  download_items_.swap(downloads);
  sort(download_items_.begin(), download_items_.end(), DownloadItemSorter());

  // Scan for any in progress downloads and add ourself to them as an observer.
  for (OrderedDownloads::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin()) > kMaxDownloads)
      break;

    DownloadItem* download = *it;
    if (download->state() == DownloadItem::IN_PROGRESS) {
      // We want to know what happens as the download progresses.
      download->AddObserver(this);
    } else if (download->safety_state() == DownloadItem::DANGEROUS) {
      // We need to be notified when the user validates the dangerous download.
      download->AddObserver(this);
    }
  }

  SendCurrentDownloads();
}

void DownloadsDOMHandler::HandleGetDownloads(const Value* value) {
  std::wstring new_search = ExtractStringValue(value);
  if (search_text_.compare(new_search) != 0) {
    search_text_ = new_search;
    ClearDownloadItems();
    download_manager_->GetDownloads(this, search_text_);
  } else {
    SendCurrentDownloads();
  }
}

void DownloadsDOMHandler::HandleOpenFile(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    download_manager_->OpenDownload(file, NULL);
}

void DownloadsDOMHandler::HandleDrag(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file) {
    IconManager* im = g_browser_process->icon_manager();
    SkBitmap* icon = im->LookupIcon(file->full_path(), IconLoader::NORMAL);
    gfx::NativeView view = dom_ui_->tab_contents()->GetNativeView();
    download_util::DragDownload(file, icon, view);
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    download_manager_->DangerousDownloadValidated(file);
}

void DownloadsDOMHandler::HandleDiscardDangerous(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->Remove(true);
}

void DownloadsDOMHandler::HandleShow(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
      download_manager_->ShowDownloadInShell(file);
}

void DownloadsDOMHandler::HandlePause(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->TogglePause();
}

void DownloadsDOMHandler::HandleRemove(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->Remove(false);
}

void DownloadsDOMHandler::HandleCancel(const Value* value) {
  DownloadItem* file = GetDownloadByValue(value);
  if (file)
    file->Cancel(true);
}

void DownloadsDOMHandler::HandleClearAll(const Value* value) {
  download_manager_->RemoveAllDownloads();
}

// DownloadsDOMHandler, private: ----------------------------------------------

void DownloadsDOMHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    int index = static_cast<int>(it - download_items_.begin());
    if (index > kMaxDownloads)
      break;
    results_value.Append(download_util::CreateDownloadItemValue(*it, index));
  }

  dom_ui_->CallJavascriptFunction(L"downloadsList", results_value);
}

void DownloadsDOMHandler::ClearDownloadItems() {
  // Clear out old state and remove self as observer for each download.
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    (*it)->RemoveObserver(this);
  }
  download_items_.clear();
}

DownloadItem* DownloadsDOMHandler::GetDownloadById(int id) {
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin() == id)) {
      return (*it);
    }
  }

  return NULL;
}

DownloadItem* DownloadsDOMHandler::GetDownloadByValue(const Value* value) {
  int id;
  if (ExtractIntegerValue(value, &id)) {
    return GetDownloadById(id);
  }
  return NULL;
}
