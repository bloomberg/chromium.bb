// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_dom_handler.h"

#include <algorithm>
#include <functional>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/gfx/image.h"

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

}  // namespace

DownloadsDOMHandler::DownloadsDOMHandler(DownloadManager* dlm)
    : search_text_(),
      download_manager_(dlm),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // Create our fileicon data source.
  dlm->profile()->GetChromeURLDataManager()->AddDataSource(
      new FileIconSource());
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
  web_ui_->RegisterMessageCallback("getDownloads",
      NewCallback(this, &DownloadsDOMHandler::HandleGetDownloads));
  web_ui_->RegisterMessageCallback("openFile",
      NewCallback(this, &DownloadsDOMHandler::HandleOpenFile));

  web_ui_->RegisterMessageCallback("drag",
      NewCallback(this, &DownloadsDOMHandler::HandleDrag));

  web_ui_->RegisterMessageCallback("saveDangerous",
      NewCallback(this, &DownloadsDOMHandler::HandleSaveDangerous));
  web_ui_->RegisterMessageCallback("discardDangerous",
      NewCallback(this, &DownloadsDOMHandler::HandleDiscardDangerous));
  web_ui_->RegisterMessageCallback("show",
      NewCallback(this, &DownloadsDOMHandler::HandleShow));
  web_ui_->RegisterMessageCallback("togglepause",
      NewCallback(this, &DownloadsDOMHandler::HandlePause));
  web_ui_->RegisterMessageCallback("resume",
      NewCallback(this, &DownloadsDOMHandler::HandlePause));
  web_ui_->RegisterMessageCallback("remove",
      NewCallback(this, &DownloadsDOMHandler::HandleRemove));
  web_ui_->RegisterMessageCallback("cancel",
      NewCallback(this, &DownloadsDOMHandler::HandleCancel));
  web_ui_->RegisterMessageCallback("clearAll",
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
  web_ui_->CallJavascriptFunction("downloadUpdated", results_value);
}

// A download has started or been deleted. Query our DownloadManager for the
// current set of downloads.
void DownloadsDOMHandler::ModelChanged() {
  ClearDownloadItems();
  download_manager_->SearchDownloads(WideToUTF16(search_text_),
                                     &download_items_);
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

void DownloadsDOMHandler::HandleGetDownloads(const ListValue* args) {
  std::wstring new_search = UTF16ToWideHack(ExtractStringValue(args));
  if (search_text_.compare(new_search) != 0) {
    search_text_ = new_search;
    ModelChanged();
  } else {
    SendCurrentDownloads();
  }
}

void DownloadsDOMHandler::HandleOpenFile(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->OpenDownload();
}

void DownloadsDOMHandler::HandleDrag(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file) {
    IconManager* im = g_browser_process->icon_manager();
    gfx::Image* icon = im->LookupIcon(file->GetUserVerifiedFilePath(),
                                      IconLoader::NORMAL);
    gfx::NativeView view = web_ui_->tab_contents()->GetNativeView();
    download_util::DragDownload(file, icon, view);
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    download_manager_->DangerousDownloadValidated(file);
}

void DownloadsDOMHandler::HandleDiscardDangerous(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Remove(true);
}

void DownloadsDOMHandler::HandleShow(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->ShowDownloadInShell();
}

void DownloadsDOMHandler::HandlePause(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->TogglePause();
}

void DownloadsDOMHandler::HandleRemove(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Remove(false);
}

void DownloadsDOMHandler::HandleCancel(const ListValue* args) {
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Cancel(true);
}

void DownloadsDOMHandler::HandleClearAll(const ListValue* args) {
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

  web_ui_->CallJavascriptFunction("downloadsList", results_value);
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

DownloadItem* DownloadsDOMHandler::GetDownloadByValue(const ListValue* args) {
  int id;
  if (ExtractIntegerValue(args, &id)) {
    return GetDownloadById(id);
  }
  return NULL;
}
