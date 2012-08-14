// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_dom_handler.h"

#include <algorithm>
#include <functional>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/gfx/image/image.h"

#if !defined(OS_MACOSX)
#include "content/public/browser/browser_thread.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::UserMetricsAction;

namespace {

// Maximum number of downloads to show. TODO(glen): Remove this and instead
// stuff the downloads down the pipe slowly.
static const int kMaxDownloads = 150;

// Sort DownloadItems into descending order by their start time.
class DownloadItemSorter : public std::binary_function<content::DownloadItem*,
                                                       content::DownloadItem*,
                                                       bool> {
 public:
  bool operator()(const content::DownloadItem* lhs,
                  const content::DownloadItem* rhs) {
    return lhs->GetStartTime() > rhs->GetStartTime();
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

// Returns a string constant to be used as the |danger_type| value in
// CreateDownloadItemValue().  We only return strings for DANGEROUS_FILE,
// DANGEROUS_URL, DANGEROUS_CONTENT, and UNCOMMON_CONTENT because the
// |danger_type| value is only defined if the value of |state| is |DANGEROUS|.
const char* GetDangerTypeString(content::DownloadDangerType danger_type) {
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE";
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS_CONTENT";
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON_CONTENT";
    default:
      // We shouldn't be returning a danger type string if it is
      // NOT_DANGEROUS or MAYBE_DANGEROUS_CONTENT.
      NOTREACHED();
      return "";
  }
}

// Return a JSON dictionary containing some of the attributes of |download|.
// The JSON dictionary will also have a field "id" set to |id|, and a field
// "otr" set to |incognito|.
DictionaryValue* CreateDownloadItemValue(
    content::DownloadItem* download,
    int id,
    bool incognito) {
  DictionaryValue* file_value = new DictionaryValue();

  file_value->SetInteger(
      "started", static_cast<int>(download->GetStartTime().ToTimeT()));
  file_value->SetString(
      "since_string", TimeFormat::RelativeDate(download->GetStartTime(), NULL));
  file_value->SetString(
      "date_string", base::TimeFormatShortDate(download->GetStartTime()));
  file_value->SetInteger("id", id);

  FilePath download_path(download->GetTargetFilePath());
  file_value->Set("file_path", base::CreateFilePathValue(download_path));
  file_value->SetString("file_url",
                        net::FilePathToFileURL(download_path).spec());

  // Keep file names as LTR.
  string16 file_name = download->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download->GetURL().spec());
  file_value->SetBoolean("otr", incognito);
  file_value->SetInteger("total", static_cast<int>(download->GetTotalBytes()));
  file_value->SetBoolean("file_externally_removed",
                         download->GetFileExternallyRemoved());

  if (download->IsInProgress()) {
    if (download->GetSafetyState() == content::DownloadItem::DANGEROUS) {
      file_value->SetString("state", "DANGEROUS");
      // These are the only danger states we expect to see (and the UI is
      // equipped to handle):
      DCHECK(download->GetDangerType() ==
                 content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
             download->GetDangerType() ==
                 content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
             download->GetDangerType() ==
                 content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
             download->GetDangerType() ==
                 content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT);
      const char* danger_type_value =
          GetDangerTypeString(download->GetDangerType());
      file_value->SetString("danger_type", danger_type_value);
    } else if (download->IsPaused()) {
      file_value->SetString("state", "PAUSED");
    } else {
      file_value->SetString("state", "IN_PROGRESS");
    }

    file_value->SetString("progress_status_text",
        download_util::GetProgressStatusText(download));

    file_value->SetInteger("percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger("received",
        static_cast<int>(download->GetReceivedBytes()));
  } else if (download->IsInterrupted()) {
    file_value->SetString("state", "INTERRUPTED");

    file_value->SetString("progress_status_text",
        download_util::GetProgressStatusText(download));

    file_value->SetInteger("percent",
        static_cast<int>(download->PercentComplete()));
    file_value->SetInteger("received",
        static_cast<int>(download->GetReceivedBytes()));
    file_value->SetString("last_reason_text",
        BaseDownloadItemModel::InterruptReasonMessage(
            download->GetLastReason()));
  } else if (download->IsCancelled()) {
    file_value->SetString("state", "CANCELLED");
  } else if (download->IsComplete()) {
    if (download->GetSafetyState() == content::DownloadItem::DANGEROUS)
      file_value->SetString("state", "DANGEROUS");
    else
      file_value->SetString("state", "COMPLETE");
  } else {
    NOTREACHED() << "state undefined";
  }

  return file_value;
}

// Return true if |download_id| refers to a download that belongs to the
// incognito download manager, if one exists.
bool IsItemIncognito(
    int32 download_id,
    content::DownloadManager* manager,
    content::DownloadManager* original_manager) {
  // |original_manager| is only non-NULL if |manager| is incognito.
  return (original_manager &&
          (manager->GetDownload(download_id) != NULL));
}

}  // namespace

DownloadsDOMHandler::DownloadsDOMHandler(content::DownloadManager* dlm)
    : search_text_(),
      download_manager_(dlm),
      original_profile_download_manager_(NULL),
      initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Create our fileicon data source.
  Profile* profile = Profile::FromBrowserContext(dlm->GetBrowserContext());
  ChromeURLDataManager::AddDataSource(profile, new FileIconSource());

  // Figure out our parent DownloadManager, if any.
  Profile* original_profile = profile->GetOriginalProfile();
  if (original_profile != profile) {
    original_profile_download_manager_ =
        BrowserContext::GetDownloadManager(original_profile);
  }
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  ClearDownloadItems();
  download_manager_->RemoveObserver(this);
  if (original_profile_download_manager_)
    original_profile_download_manager_->RemoveObserver(this);
}

// DownloadsDOMHandler, public: -----------------------------------------------

void DownloadsDOMHandler::OnPageLoaded(const base::ListValue* args) {
  if (initialized_)
    return;
  initialized_ = true;

  download_manager_->AddObserver(this);
  if (original_profile_download_manager_)
    original_profile_download_manager_->AddObserver(this);
}

void DownloadsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("onPageLoaded",
      base::Bind(&DownloadsDOMHandler::OnPageLoaded,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getDownloads",
      base::Bind(&DownloadsDOMHandler::HandleGetDownloads,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openFile",
      base::Bind(&DownloadsDOMHandler::HandleOpenFile,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("drag",
      base::Bind(&DownloadsDOMHandler::HandleDrag,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("saveDangerous",
      base::Bind(&DownloadsDOMHandler::HandleSaveDangerous,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("discardDangerous",
      base::Bind(&DownloadsDOMHandler::HandleDiscardDangerous,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("show",
      base::Bind(&DownloadsDOMHandler::HandleShow,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("togglepause",
      base::Bind(&DownloadsDOMHandler::HandlePause,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("resume",
      base::Bind(&DownloadsDOMHandler::HandlePause,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("remove",
      base::Bind(&DownloadsDOMHandler::HandleRemove,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("cancel",
      base::Bind(&DownloadsDOMHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearAll",
      base::Bind(&DownloadsDOMHandler::HandleClearAll,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openDownloadsFolder",
      base::Bind(&DownloadsDOMHandler::HandleOpenDownloadsFolder,
                 base::Unretained(this)));
}

void DownloadsDOMHandler::OnDownloadUpdated(content::DownloadItem* download) {
  // Get the id for the download. Our downloads are sorted latest to first,
  // and the id is the index into that list. We should be careful of sync
  // errors between the UI and the download_items_ list (we may wish to use
  // something other than 'id').
  OrderedDownloads::iterator it = std::find(download_items_.begin(),
                                            download_items_.end(),
                                            download);
  if (it == download_items_.end())
    return;

  const int id = static_cast<int>(it - download_items_.begin());

  ListValue results_value;
  results_value.Append(CreateDownloadItemValue(download, id, IsItemIncognito(
      download->GetId(),
      download_manager_,
      original_profile_download_manager_)));
  web_ui()->CallJavascriptFunction("downloadUpdated", results_value);
}

void DownloadsDOMHandler::OnDownloadDestroyed(
    content::DownloadItem* download) {
  download->RemoveObserver(this);
  OrderedDownloads::iterator it = std::find(download_items_.begin(),
                                            download_items_.end(),
                                            download);
  if (it != download_items_.end())
    *it = NULL;
  // A later ModelChanged() notification will change the WebUI's
  // view of the downloads list.
}

// A download has started or been deleted. Query our DownloadManager for the
// current set of downloads.
void DownloadsDOMHandler::ModelChanged(content::DownloadManager* manager) {
  DCHECK(manager == download_manager_ ||
         manager == original_profile_download_manager_);

  ClearDownloadItems();
  download_manager_->SearchDownloads(WideToUTF16(search_text_),
                                     &download_items_);
  // If we have a parent DownloadManager, let it add its downloads to the
  // results.
  if (original_profile_download_manager_) {
    original_profile_download_manager_->SearchDownloads(
        WideToUTF16(search_text_), &download_items_);
  }

  sort(download_items_.begin(), download_items_.end(), DownloadItemSorter());

  // Remove any extension downloads.
  for (size_t i = 0; i < download_items_.size();) {
    if (download_crx_util::IsExtensionDownload(*download_items_[i]))
      download_items_.erase(download_items_.begin() + i);
    else
      i++;
  }

  // Add ourself to all download items as an observer.
  for (OrderedDownloads::iterator it = download_items_.begin();
       it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin()) > kMaxDownloads)
      break;

    // We should never see anything that isn't already in the history.
    DCHECK(*it);
    DCHECK((*it)->IsPersisted());

    (*it)->AddObserver(this);
  }

  SendCurrentDownloads();
}

void DownloadsDOMHandler::ManagerGoingDown(content::DownloadManager* manager) {
  // This should never happen.  The lifetime of the DownloadsDOMHandler
  // is tied to the tab in which downloads.html is displayed, which cannot
  // outlive the Browser that contains it, which cannot outlive the Profile
  // it is associated with.  If that profile is an incognito profile,
  // it cannot outlive its original profile.  Thus this class should be
  // destroyed before a ManagerGoingDown() notification occurs.
  NOTREACHED();
}

void DownloadsDOMHandler::HandleGetDownloads(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_GET_DOWNLOADS);
  std::wstring new_search = UTF16ToWideHack(ExtractStringValue(args));
  if (search_text_.compare(new_search) != 0) {
    search_text_ = new_search;
    ModelChanged(download_manager_);
  } else {
    SendCurrentDownloads();
  }

  download_manager_->CheckForHistoryFilesRemoval();
}

void DownloadsDOMHandler::HandleOpenFile(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FILE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->OpenDownload();
}

void DownloadsDOMHandler::HandleDrag(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DRAG);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file) {
    IconManager* im = g_browser_process->icon_manager();
    gfx::Image* icon = im->LookupIcon(file->GetUserVerifiedFilePath(),
                                      IconLoader::NORMAL);
    gfx::NativeView view = web_ui()->GetWebContents()->GetNativeView();
    {
      // Enable nested tasks during DnD, while |DragDownload()| blocks.
      MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
      download_util::DragDownload(file, icon, view);
    }
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    ShowDangerPrompt(file);
  // TODO(benjhayden): else ModelChanged()? Downloads might be able to disappear
  // out from under us, so update our idea of the downloads as soon as possible.
}

void DownloadsDOMHandler::HandleDiscardDangerous(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Delete(content::DownloadItem::DELETE_DUE_TO_USER_DISCARD);
}

void DownloadsDOMHandler::HandleShow(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SHOW);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->ShowDownloadInShell();
}

void DownloadsDOMHandler::HandlePause(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_PAUSE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->TogglePause();
}

void DownloadsDOMHandler::HandleRemove(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REMOVE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file) {
    DCHECK(file->IsPersisted());
    file->Remove();
  }
}

void DownloadsDOMHandler::HandleCancel(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Cancel(true);
}

void DownloadsDOMHandler::HandleClearAll(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CLEAR_ALL);
  download_manager_->RemoveAllDownloads();

  // If this is an incognito downloader, clear All should clear main download
  // manager as well.
  if (original_profile_download_manager_)
    original_profile_download_manager_->RemoveAllDownloads();
}

void DownloadsDOMHandler::HandleOpenDownloadsFolder(const ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FOLDER);
  platform_util::OpenItem(
      DownloadPrefs::FromDownloadManager(download_manager_)->DownloadPath());
}

// DownloadsDOMHandler, private: ----------------------------------------------

void DownloadsDOMHandler::SendCurrentDownloads() {
  ListValue results_value;
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    if (!*it)
      continue;
    int index = static_cast<int>(it - download_items_.begin());
    if (index <= kMaxDownloads)
      results_value.Append(CreateDownloadItemValue(*it, index, IsItemIncognito(
            (*it)->GetId(),
            download_manager_,
            original_profile_download_manager_)));
  }

  web_ui()->CallJavascriptFunction("downloadsList", results_value);
}

void DownloadsDOMHandler::ShowDangerPrompt(
    content::DownloadItem* dangerous_item) {
  DownloadDangerPrompt* danger_prompt = DownloadDangerPrompt::Create(
      dangerous_item,
      TabContents::FromWebContents(web_ui()->GetWebContents()),
      base::Bind(&DownloadsDOMHandler::DangerPromptAccepted,
                 weak_ptr_factory_.GetWeakPtr(), dangerous_item->GetId()),
      base::Closure());
  // danger_prompt will delete itself.
  DCHECK(danger_prompt);
}

void DownloadsDOMHandler::DangerPromptAccepted(int download_id) {
  content::DownloadItem* item = download_manager_->GetActiveDownloadItem(
      download_id);
  if (!item)
    return;
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  item->DangerousDownloadValidated();
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

content::DownloadItem* DownloadsDOMHandler::GetDownloadById(int id) {
  for (OrderedDownloads::iterator it = download_items_.begin();
      it != download_items_.end(); ++it) {
    if (static_cast<int>(it - download_items_.begin() == id)) {
      return (*it);
    }
  }

  return NULL;
}

content::DownloadItem* DownloadsDOMHandler::GetDownloadByValue(
    const ListValue* args) {
  int id;
  if (ExtractIntegerValue(args, &id)) {
    return GetDownloadById(id);
  }
  return NULL;
}
