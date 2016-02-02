// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads_dom_handler.h"

#include <stddef.h>

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/threading/thread.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "net/base/filename_util.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image.h"

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;

namespace {

// Maximum number of downloads to show. TODO(glen): Remove this and instead
// stuff the downloads down the pipe slowly.
size_t kMaxNumberOfDownloads = 150;

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
  DOWNLOADS_DOM_EVENT_MAX
};

void CountDownloadsDOMEvents(DownloadsDOMEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.DOMEvent",
                            event,
                            DOWNLOADS_DOM_EVENT_MAX);
}

// Returns a string constant to be used as the |danger_type| value in
// CreateDownloadItemValue().  Only return strings for DANGEROUS_FILE,
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
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    default:
      // Don't return a danger type string if it is NOT_DANGEROUS or
      // MAYBE_DANGEROUS_CONTENT.
      NOTREACHED();
      return "";
  }
}

// Returns a JSON dictionary containing some of the attributes of |download|.
// The JSON dictionary will also have a field "id" set to |id|, and a field
// "otr" set to |incognito|.
base::DictionaryValue* CreateDownloadItemValue(
    content::DownloadItem* download_item,
    bool incognito) {
  // TODO(asanka): Move towards using download_model here for getting status and
  // progress. The difference currently only matters to Drive downloads and
  // those don't show up on the downloads page, but should.
  DownloadItemModel download_model(download_item);

  // The items which are to be written into file_value are also described in
  // chrome/browser/resources/downloads/downloads.js in @typedef for
  // BackendDownloadObject. Please update it whenever you add or remove
  // any keys in file_value.
  base::DictionaryValue* file_value = new base::DictionaryValue();

  file_value->SetInteger(
      "started", static_cast<int>(download_item->GetStartTime().ToTimeT()));
  file_value->SetString(
      "since_string", ui::TimeFormat::RelativeDate(
          download_item->GetStartTime(), NULL));

  base::Time start_time = download_item->GetStartTime();
  base::string16 date_string = base::TimeFormatShortDate(start_time);
  file_value->SetString("date_string", date_string);

  file_value->SetString("id", base::Uint64ToString(download_item->GetId()));

  base::FilePath download_path(download_item->GetTargetFilePath());
  file_value->Set("file_path", base::CreateFilePathValue(download_path));
  file_value->SetString("file_url",
                        net::FilePathToFileURL(download_path).spec());

  extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(download_item);
  std::string by_ext_id;
  std::string by_ext_name;
  if (by_ext) {
    by_ext_id = by_ext->id();
    // TODO(dbeam): why doesn't DownloadsByExtension::name() return a string16?
    by_ext_name = by_ext->name();

    // Lookup the extension's current name() in case the user changed their
    // language. This won't work if the extension was uninstalled, so the name
    // might be the wrong language.
    bool include_disabled = true;
    const extensions::Extension* extension = extensions::ExtensionSystem::Get(
        Profile::FromBrowserContext(download_item->GetBrowserContext()))->
      extension_service()->GetExtensionById(by_ext->id(), include_disabled);
    if (extension)
      file_value->SetString("by_ext_name", extension->name());
  }
  file_value->SetString("by_ext_id", by_ext_id);
  file_value->SetString("by_ext_name", by_ext_name);

  // Keep file names as LTR.
  base::string16 file_name =
    download_item->GetFileNameToReportUser().LossyDisplayName();
  file_name = base::i18n::GetDisplayStringInLTRDirectionality(file_name);
  file_value->SetString("file_name", file_name);
  file_value->SetString("url", download_item->GetURL().spec());
  file_value->SetBoolean("otr", incognito);
  file_value->SetInteger("total", static_cast<int>(
      download_item->GetTotalBytes()));
  file_value->SetBoolean("file_externally_removed",
                         download_item->GetFileExternallyRemoved());
  file_value->SetBoolean("resume", download_item->CanResume());

  const char* danger_type = "";
  base::string16 last_reason_text;
  // -2 is invalid, -1 means indeterminate, and 0-100 are in-progress.
  int percent = -2;
  base::string16 progress_status_text;
  bool retry = false;
  const char* state = nullptr;

  switch (download_item->GetState()) {
    case content::DownloadItem::IN_PROGRESS: {
      if (download_item->IsDangerous()) {
        state = "DANGEROUS";
        // These are the only danger states that the UI is equipped to handle.
        DCHECK(download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
               download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
               download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
               download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
               download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
               download_item->GetDangerType() ==
                   content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
        danger_type = GetDangerTypeString(download_item->GetDangerType());
      } else if (download_item->IsPaused()) {
        state = "PAUSED";
      } else {
        state = "IN_PROGRESS";
      }
      progress_status_text = download_model.GetTabProgressStatusText();
      percent = std::max(0, download_item->PercentComplete());
      break;
    }

    case content::DownloadItem::INTERRUPTED:
      state = "INTERRUPTED";
      progress_status_text = download_model.GetTabProgressStatusText();

      if (download_item->CanResume())
        percent = download_item->PercentComplete();

      last_reason_text = download_model.GetInterruptReasonText();
      if (content::DOWNLOAD_INTERRUPT_REASON_CRASH ==
          download_item->GetLastReason() && !download_item->CanResume()) {
        retry = true;
      }
      break;

    case content::DownloadItem::CANCELLED:
      state = "CANCELLED";
      retry = true;
      break;

    case content::DownloadItem::COMPLETE:
      DCHECK(!download_item->IsDangerous());
      state = "COMPLETE";
      break;

    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }

  DCHECK(state);

  file_value->SetString("danger_type", danger_type);
  file_value->SetString("last_reason_text", last_reason_text);
  file_value->SetInteger("percent", percent);
  file_value->SetString("progress_status_text", progress_status_text);
  file_value->SetBoolean("retry", retry);
  file_value->SetString("state", state);

  return file_value;
}

// Filters out extension downloads and downloads that don't have a filename yet.
bool IsDownloadDisplayable(const content::DownloadItem& item) {
  return !download_crx_util::IsExtensionDownload(item) &&
         !item.IsTemporary() &&
         !item.GetFileNameToReportUser().empty() &&
         !item.GetTargetFilePath().empty() &&
         DownloadItemModel(
             const_cast<content::DownloadItem*>(&item)).ShouldShowInShelf();
}

}  // namespace

DownloadsDOMHandler::DownloadsDOMHandler(
    content::DownloadManager* download_manager)
    : download_manager_(download_manager),
      update_scheduled_(false),
      weak_ptr_factory_(this) {
  // Create our fileicon data source.
  Profile* profile = Profile::FromBrowserContext(
      download_manager->GetBrowserContext());
  content::URLDataSource::Add(profile, new FileIconSource());
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  FinalizeRemovals();
}

// DownloadsDOMHandler, public: -----------------------------------------------

void DownloadsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getDownloads",
      base::Bind(&DownloadsDOMHandler::HandleGetDownloads,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("openFile",
      base::Bind(&DownloadsDOMHandler::HandleOpenFile,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("drag",
      base::Bind(&DownloadsDOMHandler::HandleDrag,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("saveDangerous",
      base::Bind(&DownloadsDOMHandler::HandleSaveDangerous,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("discardDangerous",
      base::Bind(&DownloadsDOMHandler::HandleDiscardDangerous,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("show",
      base::Bind(&DownloadsDOMHandler::HandleShow,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("pause",
      base::Bind(&DownloadsDOMHandler::HandlePause,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("resume",
      base::Bind(&DownloadsDOMHandler::HandleResume,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("remove",
      base::Bind(&DownloadsDOMHandler::HandleRemove,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("undo",
      base::Bind(&DownloadsDOMHandler::HandleUndo,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("cancel",
      base::Bind(&DownloadsDOMHandler::HandleCancel,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("clearAll",
      base::Bind(&DownloadsDOMHandler::HandleClearAll,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback("openDownloadsFolder",
      base::Bind(&DownloadsDOMHandler::HandleOpenDownloadsFolder,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DownloadsDOMHandler::OnDownloadCreated(
    content::DownloadManager* manager, content::DownloadItem* download_item) {
  if (IsDownloadDisplayable(*download_item))
    ScheduleSendCurrentDownloads();
  else
    new_downloads_.insert(download_item->GetId());
}

void DownloadsDOMHandler::OnDownloadUpdated(
    content::DownloadManager* manager,
    content::DownloadItem* download_item) {
  if (update_scheduled_)
    return;

  bool showing_new_item = false;

  if (new_downloads_.count(download_item->GetId())) {
    // A new download (that the page doesn't know about yet) has been updated.
    if (!IsDownloadDisplayable(*download_item)) {
      // Item isn't ready to be displayed yet. Wait until it is.
      return;
    }

    new_downloads_.erase(download_item->GetId());
    showing_new_item = true;
  }

  if (showing_new_item || DownloadItemModel(download_item).IsBeingRevived() ||
      !IsDownloadDisplayable(*download_item)) {
    // A download will be shown or hidden by this update. Resend the list.
    ScheduleSendCurrentDownloads();
    return;
  }

  if (search_terms_ && !search_terms_->empty()) {
    // Don't CallUpdateItem() if download_item doesn't match
    // search_terms_.
    // TODO(benjhayden): Consider splitting MatchesQuery() out to a function.
    content::DownloadManager::DownloadVector all_items, filtered_items;
    all_items.push_back(download_item);
    DownloadQuery query;
    query.AddFilter(DownloadQuery::FILTER_QUERY, *search_terms_);
    query.Search(all_items.begin(), all_items.end(), &filtered_items);
    if (filtered_items.empty())
      return;
  }

  DCHECK(manager);
  scoped_ptr<base::DictionaryValue> item(CreateDownloadItemValue(
      download_item,
      original_notifier_ && manager == GetMainNotifierManager()));
  CallUpdateItem(*item);
}

void DownloadsDOMHandler::OnDownloadRemoved(
    content::DownloadManager* manager,
    content::DownloadItem* download_item) {
  if (!DownloadItemModel(download_item).ShouldShowInShelf())
    return;

  // This relies on |download_item| being removed from DownloadManager in this
  // MessageLoop iteration. |download_item| may not have been removed from
  // DownloadManager when OnDownloadRemoved() is fired, so bounce off the
  // MessageLoop to give it a chance to be removed. SendCurrentDownloads() looks
  // at all downloads, and we do not tell it that |download_item| is being
  // removed. If DownloadManager is ever changed to not immediately remove
  // |download_item| from its map when OnDownloadRemoved is sent, then
  // DownloadsDOMHandler::OnDownloadRemoved() will need to explicitly tell
  // SendCurrentDownloads() that |download_item| was removed. A
  // SupportsUserData::Data would be the correct way to do this.
  ScheduleSendCurrentDownloads();
}

void DownloadsDOMHandler::HandleGetDownloads(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_GET_DOWNLOADS);
  search_terms_.reset(args && !args->empty() ? args->DeepCopy() : NULL);
  ScheduleSendCurrentDownloads();

  if (!main_notifier_) {
    main_notifier_.reset(new AllDownloadItemNotifier(download_manager_, this));

    Profile* profile = Profile::FromBrowserContext(
        download_manager_->GetBrowserContext());
    if (profile->IsOffTheRecord()) {
      original_notifier_.reset(new AllDownloadItemNotifier(
          BrowserContext::GetDownloadManager(profile->GetOriginalProfile()),
          this));
    }
  }
}

void DownloadsDOMHandler::HandleOpenFile(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FILE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->OpenDownload();
}

void DownloadsDOMHandler::HandleDrag(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DRAG);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (!file)
    return;

  content::WebContents* web_contents = GetWebUIWebContents();
  // |web_contents| is only NULL in the test.
  if (!web_contents)
    return;

  if (file->GetState() != content::DownloadItem::COMPLETE)
    return;

  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      file->GetTargetFilePath(), IconLoader::NORMAL);
  gfx::NativeView view = web_contents->GetNativeView();
  {
    // Enable nested tasks during DnD, while |DragDownload()| blocks.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    DragDownloadItem(file, icon, view);
  }
}

void DownloadsDOMHandler::HandleSaveDangerous(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    ShowDangerPrompt(file);
}

void DownloadsDOMHandler::HandleDiscardDangerous(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Remove();
}

void DownloadsDOMHandler::HandleShow(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SHOW);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->ShowDownloadInShell();
}

void DownloadsDOMHandler::HandlePause(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_PAUSE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Pause();
}

void DownloadsDOMHandler::HandleResume(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_RESUME);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Resume();
}

void DownloadsDOMHandler::HandleRemove(const base::ListValue* args) {
  if (!IsDeletingHistoryAllowed())
    return;

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REMOVE);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (!file)
    return;

  std::vector<content::DownloadItem*> downloads;
  downloads.push_back(file);
  RemoveDownloads(downloads);
}

void DownloadsDOMHandler::HandleUndo(const base::ListValue* args) {
  // TODO(dbeam): handle more than removed downloads someday?
  if (removals_.empty())
    return;

  const std::set<uint32_t> last_removed_ids = removals_.back();
  removals_.pop_back();

  for (auto id : last_removed_ids) {
    content::DownloadItem* download = GetDownloadById(id);
    if (!download)
      continue;

    DownloadItemModel model(download);
    model.SetShouldShowInShelf(true);
    model.SetIsBeingRevived(true);

    download->UpdateObservers();

    model.SetIsBeingRevived(false);
  }
}

void DownloadsDOMHandler::HandleCancel(const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL);
  content::DownloadItem* file = GetDownloadByValue(args);
  if (file)
    file->Cancel(true);
}

void DownloadsDOMHandler::HandleClearAll(const base::ListValue* args) {
  if (!IsDeletingHistoryAllowed()) {
    // This should only be reached during tests.
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CLEAR_ALL);

  std::vector<content::DownloadItem*> downloads;
  if (GetMainNotifierManager())
    GetMainNotifierManager()->GetAllDownloads(&downloads);
  if (GetOriginalNotifierManager())
    GetOriginalNotifierManager()->GetAllDownloads(&downloads);
  RemoveDownloads(downloads);
}

void DownloadsDOMHandler::RemoveDownloads(
    const std::vector<content::DownloadItem*>& to_remove) {
  std::set<uint32_t> ids;

  for (auto* download : to_remove) {
    DownloadItemModel item_model(download);
    if (!item_model.ShouldShowInShelf() ||
        download->GetState() == content::DownloadItem::IN_PROGRESS) {
      continue;
    }

    item_model.SetShouldShowInShelf(false);
    ids.insert(download->GetId());
    download->UpdateObservers();
  }

  if (!ids.empty())
    removals_.push_back(ids);
}

void DownloadsDOMHandler::HandleOpenDownloadsFolder(
    const base::ListValue* args) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FOLDER);
  content::DownloadManager* manager = GetMainNotifierManager();
  if (manager) {
    platform_util::OpenItem(
        Profile::FromBrowserContext(manager->GetBrowserContext()),
        DownloadPrefs::FromDownloadManager(manager)->DownloadPath(),
        platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());
  }
}

// DownloadsDOMHandler, private: ----------------------------------------------

void DownloadsDOMHandler::ScheduleSendCurrentDownloads() {
  // Don't call SendCurrentDownloads() every time anything changes. Batch them
  // together instead. This may handle hundreds of OnDownloadDestroyed() calls
  // in a single UI message loop iteration when the user Clears All downloads.
  if (update_scheduled_)
    return;

  update_scheduled_ = true;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadsDOMHandler::SendCurrentDownloads,
                 weak_ptr_factory_.GetWeakPtr()));
}

content::DownloadManager* DownloadsDOMHandler::GetMainNotifierManager() const {
  return main_notifier_ ? main_notifier_->GetManager() : nullptr;
}

content::DownloadManager* DownloadsDOMHandler::GetOriginalNotifierManager()
    const {
  return original_notifier_ ? original_notifier_->GetManager() : nullptr;
}

void DownloadsDOMHandler::FinalizeRemovals() {
  while (!removals_.empty()) {
    const std::set<uint32_t> remove = removals_.back();
    removals_.pop_back();

    for (const auto id : remove) {
      content::DownloadItem* download = GetDownloadById(id);
      if (download)
        download->Remove();
    }
  }
}

void DownloadsDOMHandler::SendCurrentDownloads() {
  update_scheduled_ = false;

  content::DownloadManager::DownloadVector all_items, filtered_items;
  if (GetMainNotifierManager()) {
    GetMainNotifierManager()->GetAllDownloads(&all_items);
    GetMainNotifierManager()->CheckForHistoryFilesRemoval();
  }
  if (GetOriginalNotifierManager()) {
    GetOriginalNotifierManager()->GetAllDownloads(&all_items);
    GetOriginalNotifierManager()->CheckForHistoryFilesRemoval();
  }

  DownloadQuery query;
  if (search_terms_ && !search_terms_->empty())
    query.AddFilter(DownloadQuery::FILTER_QUERY, *search_terms_);
  query.AddFilter(base::Bind(&IsDownloadDisplayable));
  query.AddSorter(DownloadQuery::SORT_START_TIME, DownloadQuery::DESCENDING);
  query.Limit(kMaxNumberOfDownloads);
  query.Search(all_items.begin(), all_items.end(), &filtered_items);

  base::ListValue results_value;
  for (auto* item : filtered_items) {
    results_value.Append(CreateDownloadItemValue(
        item,
        original_notifier_ && GetMainNotifierManager() &&
            GetMainNotifierManager()->GetDownload(item->GetId()) == item));
  }
  CallUpdateAll(results_value);
}

void DownloadsDOMHandler::ShowDangerPrompt(
    content::DownloadItem* dangerous_item) {
  DownloadDangerPrompt* danger_prompt = DownloadDangerPrompt::Create(
      dangerous_item,
      GetWebUIWebContents(),
      false,
      base::Bind(&DownloadsDOMHandler::DangerPromptDone,
                 weak_ptr_factory_.GetWeakPtr(), dangerous_item->GetId()));
  // danger_prompt will delete itself.
  DCHECK(danger_prompt);
}

void DownloadsDOMHandler::DangerPromptDone(
    int download_id, DownloadDangerPrompt::Action action) {
  if (action != DownloadDangerPrompt::ACCEPT)
    return;
  content::DownloadItem* item = NULL;
  if (GetMainNotifierManager())
    item = GetMainNotifierManager()->GetDownload(download_id);
  if (!item && GetOriginalNotifierManager())
    item = GetOriginalNotifierManager()->GetDownload(download_id);
  if (!item || item->IsDone())
    return;
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS);
  item->ValidateDangerousDownload();
}

bool DownloadsDOMHandler::IsDeletingHistoryAllowed() {
  content::DownloadManager* manager = GetMainNotifierManager();
  return manager &&
         Profile::FromBrowserContext(manager->GetBrowserContext())->
             GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

content::DownloadItem* DownloadsDOMHandler::GetDownloadByValue(
    const base::ListValue* args) {
  std::string download_id;
  if (!args->GetString(0, &download_id)) {
    NOTREACHED();
    return nullptr;
  }

  uint64_t id;
  if (!base::StringToUint64(download_id, &id)) {
    NOTREACHED();
    return nullptr;
  }

  return GetDownloadById(static_cast<uint32_t>(id));
}

content::DownloadItem* DownloadsDOMHandler::GetDownloadById(uint32_t id) {
  content::DownloadItem* item = NULL;
  if (GetMainNotifierManager())
    item = GetMainNotifierManager()->GetDownload(id);
  if (!item && GetOriginalNotifierManager())
    item = GetOriginalNotifierManager()->GetDownload(id);
  return item;
}

content::WebContents* DownloadsDOMHandler::GetWebUIWebContents() {
  return web_ui()->GetWebContents();
}

void DownloadsDOMHandler::CallUpdateAll(const base::ListValue& list) {
  web_ui()->CallJavascriptFunction("downloads.Manager.updateAll", list);
}

void DownloadsDOMHandler::CallUpdateItem(const base::DictionaryValue& item) {
  web_ui()->CallJavascriptFunction("downloads.Manager.updateItem", item);
}
