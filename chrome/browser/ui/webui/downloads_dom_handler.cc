// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_dom_handler.h"

#include <algorithm>
#include <functional>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/browser/ui/webui/fileicon_source_chromeos.h"
#include "chrome/common/url_constants.h"
#include "content/browser/download/download_item.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/gfx/image/image.h"

#if !defined(OS_MACOSX)
#include "content/public/browser/browser_thread.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/file_manager_util.h"
#endif

using content::BrowserThread;

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
  DOWNLOADS_DOM_EVENT_MAX
};

void CountDownloadsDOMEvents(DownloadsDOMEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.DOMEvent",
                            event,
                            DOWNLOADS_DOM_EVENT_MAX);
}

}  // namespace

class DownloadsDOMHandler::OriginalDownloadManagerObserver
    : public DownloadManager::Observer {
 public:
  explicit OriginalDownloadManagerObserver(
      DownloadManager::Observer* observer,
      Profile* original_profile)
      : observer_(observer) {
    original_profile_download_manager_ =
        DownloadServiceFactory::GetForProfile(
            original_profile)->GetDownloadManager();
    original_profile_download_manager_->AddObserver(this);
  }

  virtual ~OriginalDownloadManagerObserver() {
    if (original_profile_download_manager_)
      original_profile_download_manager_->RemoveObserver(this);
  }

  // Observer interface.
  virtual void ModelChanged() {
    observer_->ModelChanged();
  }

  virtual void ManagerGoingDown() {
    original_profile_download_manager_ = NULL;
  }

 private:
  // The DownloadsDOMHandler for the off-the-record profile.
  DownloadManager::Observer* observer_;

  // The original profile's download manager.
  DownloadManager* original_profile_download_manager_;
};

DownloadsDOMHandler::DownloadsDOMHandler(DownloadManager* dlm)
    : search_text_(),
      download_manager_(dlm),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  // Create our fileicon data source.
  Profile::FromBrowserContext(dlm->BrowserContext())->
      GetChromeURLDataManager()->AddDataSource(
#if defined(OS_CHROMEOS)
      new FileIconSourceCros());
#else
      new FileIconSource());
#endif  // OS_CHROMEOS
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
}

// DownloadsDOMHandler, public: -----------------------------------------------

void DownloadsDOMHandler::Init() {
  download_manager_->AddObserver(this);

  Profile* profile =
      Profile::FromBrowserContext(download_manager_->BrowserContext());
  Profile* original_profile = profile->GetOriginalProfile();
  if (original_profile != profile) {
    original_download_manager_observer_.reset(
        new OriginalDownloadManagerObserver(this, original_profile));
  }
}

void DownloadsDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getDownloads",
      base::Bind(&DownloadsDOMHandler::HandleGetDownloads,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("openFile",
      base::Bind(&DownloadsDOMHandler::HandleOpenFile,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("drag",
      base::Bind(&DownloadsDOMHandler::HandleDrag,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("saveDangerous",
      base::Bind(&DownloadsDOMHandler::HandleSaveDangerous,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("discardDangerous",
      base::Bind(&DownloadsDOMHandler::HandleDiscardDangerous,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("show",
      base::Bind(&DownloadsDOMHandler::HandleShow,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("togglepause",
      base::Bind(&DownloadsDOMHandler::HandlePause,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("resume",
      base::Bind(&DownloadsDOMHandler::HandlePause,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("remove",
      base::Bind(&DownloadsDOMHandler::HandleRemove,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("cancel",
      base::Bind(&DownloadsDOMHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("clearAll",
      base::Bind(&DownloadsDOMHandler::HandleClearAll,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("openDownloadsFolder",
      base::Bind(&DownloadsDOMHandler::HandleOpenDownloadsFolder,
                 base::Unretained(this)));
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

  if (download->state() == DownloadItem::REMOVING) {
    (*it)->RemoveObserver(this);
    *it = NULL;
    // A later ModelChanged() notification will change the WebUI's
    // view of the downloads list.
    return;
  }

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
  // If we have a parent profile, let it add its downloads to the results.
  Profile* profile =
      Profile::FromBrowserContext(download_manager_->BrowserContext());
  if (profile->GetOriginalProfile() != profile) {
    DownloadServiceFactory::GetForProfile(
        profile->GetOriginalProfile())->GetDownloadManager()->SearchDownloads(
            WideToUTF16(search_text_), &download_items_);
  }

  sort(download_items_.begin(), download_items_.end(), DownloadItemSorter());

  // Remove any extension downloads.
  for (size_t i = 0; i < download_items_.size();) {
    if (ChromeDownloadManagerDelegate::IsExtensionDownload(download_items_[i]))
      download_items_.erase(download_items_.begin() + i);
    else
      i++;
  }

  // Add ourself to all download items as an observer.
  for (OrderedDownloads::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin()) > kMaxDownloads)
      break;

    // TODO(rdsmith): Convert to DCHECK()s when http://crbug.com/85408 is
    // fixed.
    // We should never see anything that isn't already in the history.
    CHECK(*it);
    CHECK_NE(DownloadItem::kUninitializedHandle, (*it)->db_handle());

    (*it)->AddObserver(this);
  }

  SendCurrentDownloads();
}

void DownloadsDOMHandler::HandleGetDownloads(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_GET_DOWNLOADS);
  std::wstring new_search = UTF16ToWideHack(ExtractStringValue(args));
  if (search_text_.compare(new_search) != 0) {
    search_text_ = new_search;
    ModelChanged();
  } else {
    SendCurrentDownloads();
  }

  download_manager_->CheckForHistoryFilesRemoval();
}

void DownloadsDOMHandler::HandleOpenFile(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FILE);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->OpenDownload();
}

void DownloadsDOMHandler::HandleDrag(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DRAG);
  DownloadItem* file = GetDownloadByValue(args);
  if (file) {
    IconManager* im = g_browser_process->icon_manager();
    gfx::Image* icon = im->LookupIcon(file->GetUserVerifiedFilePath(),
                                      IconLoader::NORMAL);
    gfx::NativeView view = web_ui_->tab_contents()->GetNativeView();
    {
      // Enable nested tasks during DnD, while |DragDownload()| blocks.
      MessageLoop::ScopedNestableTaskAllower allower(MessageLoop::current());
      download_util::DragDownload(file, icon, view);
    }
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->DangerousDownloadValidated();
}

void DownloadsDOMHandler::HandleDiscardDangerous(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
}

void DownloadsDOMHandler::HandleShow(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SHOW);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->ShowDownloadInShell();
}

void DownloadsDOMHandler::HandlePause(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_PAUSE);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->TogglePause();
}

void DownloadsDOMHandler::HandleRemove(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REMOVE);
  DownloadItem* file = GetDownloadByValue(args);
  if (file) {
    // TODO(rdsmith): Change to DCHECK when http://crbug.com/85408 is fixed.
    CHECK_NE(DownloadItem::kUninitializedHandle, file->db_handle());
    file->Remove();
  }
}

void DownloadsDOMHandler::HandleCancel(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL);
  DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Cancel(true);
}

void DownloadsDOMHandler::HandleClearAll(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CLEAR_ALL);
  download_manager_->RemoveAllDownloads();

  Profile* profile =
      Profile::FromBrowserContext(download_manager_->BrowserContext());
  // If this is an incognito downloader, clear All should clear main download
  // manager as well.
  if (profile->GetOriginalProfile() != profile)
    DownloadServiceFactory::GetForProfile(
        profile->GetOriginalProfile())->
            GetDownloadManager()->RemoveAllDownloads();
}

void DownloadsDOMHandler::HandleOpenDownloadsFolder(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FOLDER);
  platform_util::OpenItem(
      DownloadPrefs::FromDownloadManager(download_manager_)->download_path());
}

// DownloadsDOMHandler, private: ----------------------------------------------

void DownloadsDOMHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    int index = static_cast<int>(it - download_items_.begin());
    if (index > kMaxDownloads)
      break;
    if (!*it)
      continue;
    results_value.Append(download_util::CreateDownloadItemValue(*it, index));
  }

  web_ui_->CallJavascriptFunction("downloadsList", results_value);
}

void DownloadsDOMHandler::ClearDownloadItems() {
  // Clear out old state and remove self as observer for each download.
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    if (!*it)
      continue;
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
