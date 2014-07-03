// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf_context_menu.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using content::DownloadItem;

namespace {

// Returns true if downloads resumption is enabled.
bool IsDownloadResumptionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDownloadResumption);
}

}  // namespace

DownloadShelfContextMenu::~DownloadShelfContextMenu() {
  DetachFromDownloadItem();
}

DownloadShelfContextMenu::DownloadShelfContextMenu(
    DownloadItem* download_item,
    content::PageNavigator* navigator)
    : download_item_(download_item),
      navigator_(navigator) {
  DCHECK(download_item_);
  download_item_->AddObserver(this);

#if defined(OS_WIN)
  is_pdf_reader_up_to_date_ = false;
  if (IsDownloadPdf() && IsAdobeReaderDefaultPDFViewer()) {
    is_pdf_reader_up_to_date_ =
        DownloadTargetDeterminer::IsAdobeReaderUpToDate();
  }
#endif  // defined(OS_WIN)
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMenuModel() {
  ui::SimpleMenuModel* model = NULL;

  if (!download_item_)
    return NULL;

  DownloadItemModel download_model(download_item_);
  // We shouldn't be opening a context menu for a dangerous download, unless it
  // is a malicious download.
  DCHECK(!download_model.IsDangerous() || download_model.MightBeMalicious());

  if (download_model.IsMalicious())
    model = GetMaliciousMenuModel();
  else if (download_model.MightBeMalicious())
    model = GetMaybeMaliciousMenuModel();
  else if (download_item_->GetState() == DownloadItem::COMPLETE)
    model = GetFinishedMenuModel();
  else if (download_item_->GetState() == DownloadItem::INTERRUPTED)
    model = GetInterruptedMenuModel();
  else
    model = GetInProgressMenuModel();
  return model;
}

bool DownloadShelfContextMenu::IsCommandIdEnabled(int command_id) const {
  if (!download_item_)
    return false;

  switch (static_cast<ContextMenuCommands>(command_id)) {
    case SHOW_IN_FOLDER:
      return download_item_->CanShowInFolder();
    case OPEN_WHEN_COMPLETE:
    case PLATFORM_OPEN:
      return download_item_->CanOpenDownload() &&
          !download_crx_util::IsExtensionDownload(*download_item_);
    case ALWAYS_OPEN_TYPE:
      // For temporary downloads, the target filename might be a temporary
      // filename. Don't base an "Always open" decision based on it. Also
      // exclude extensions.
      return download_item_->CanOpenDownload() &&
          !download_crx_util::IsExtensionDownload(*download_item_);
    case CANCEL:
      return !download_item_->IsDone();
    case TOGGLE_PAUSE:
      return !download_item_->IsDone();
    case DISCARD:
    case KEEP:
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
      return true;
  }
  NOTREACHED();
  return false;
}

bool DownloadShelfContextMenu::IsCommandIdChecked(int command_id) const {
  if (!download_item_)
    return false;

  switch (command_id) {
    case OPEN_WHEN_COMPLETE:
      return download_item_->GetOpenWhenComplete() ||
          download_crx_util::IsExtensionDownload(*download_item_);
    case ALWAYS_OPEN_TYPE:
#if defined(OS_WIN)
      if (CanOpenPdfInReader()) {
        DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
            download_item_->GetBrowserContext());
        return prefs->ShouldOpenPdfInAdobeReader();
      }
#endif
      return download_item_->ShouldOpenFileBasedOnExtension();
    case TOGGLE_PAUSE:
      return download_item_->IsPaused();
  }
  return false;
}

bool DownloadShelfContextMenu::IsCommandIdVisible(int command_id) const {
  if (!download_item_)
    return false;

  if (command_id == PLATFORM_OPEN)
    return (DownloadItemModel(download_item_).ShouldPreferOpeningInBrowser());

  return true;
}

void DownloadShelfContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (!download_item_)
    return;

  switch (static_cast<ContextMenuCommands>(command_id)) {
    case SHOW_IN_FOLDER:
      download_item_->ShowDownloadInShell();
      break;
    case OPEN_WHEN_COMPLETE:
      download_item_->OpenDownload();
      break;
    case ALWAYS_OPEN_TYPE: {
      bool is_checked = IsCommandIdChecked(ALWAYS_OPEN_TYPE);
      DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
          download_item_->GetBrowserContext());
#if defined(OS_WIN)
      if (CanOpenPdfInReader()) {
        prefs->SetShouldOpenPdfInAdobeReader(!is_checked);
        DownloadItemModel(download_item_).SetShouldPreferOpeningInBrowser(
            is_checked);
        break;
      }
#endif
      base::FilePath path = download_item_->GetTargetFilePath();
      if (is_checked)
        prefs->DisableAutoOpenBasedOnExtension(path);
      else
        prefs->EnableAutoOpenBasedOnExtension(path);
      break;
    }
    case PLATFORM_OPEN:
      DownloadItemModel(download_item_).OpenUsingPlatformHandler();
      break;
    case CANCEL:
      download_item_->Cancel(true /* Cancelled by user */);
      break;
    case TOGGLE_PAUSE:
      if (download_item_->GetState() == DownloadItem::IN_PROGRESS &&
          !download_item_->IsPaused()) {
        download_item_->Pause();
      } else {
        download_item_->Resume();
      }
      break;
    case DISCARD:
      download_item_->Remove();
      break;
    case KEEP:
      download_item_->ValidateDangerousDownload();
      break;
    case LEARN_MORE_SCANNING: {
#if defined(FULL_SAFE_BROWSING)
      using safe_browsing::DownloadProtectionService;
      SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      DownloadProtectionService* protection_service =
          (sb_service ? sb_service->download_protection_service() : NULL);
      if (protection_service) {
        protection_service->ShowDetailsForDownload(*download_item_, navigator_);
      }
#else
      // Should only be getting invoked if we are using safe browsing.
      NOTREACHED();
#endif
      break;
    }
    case LEARN_MORE_INTERRUPTED:
      navigator_->OpenURL(
          content::OpenURLParams(GURL(chrome::kDownloadInterruptedLearnMoreURL),
                                 content::Referrer(),
                                 NEW_FOREGROUND_TAB,
                                 content::PAGE_TRANSITION_LINK,
                                 false));
      break;
  }
}

bool DownloadShelfContextMenu::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

bool DownloadShelfContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PAUSE;
}

base::string16 DownloadShelfContextMenu::GetLabelForCommandId(
    int command_id) const {
  switch (static_cast<ContextMenuCommands>(command_id)) {
    case SHOW_IN_FOLDER:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_SHOW);
    case OPEN_WHEN_COMPLETE:
      if (download_item_ && !download_item_->IsDone())
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_OPEN);
    case ALWAYS_OPEN_TYPE:
      return l10n_util::GetStringUTF16(GetAlwaysOpenStringId());
    case PLATFORM_OPEN:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_PLATFORM_OPEN);
    case CANCEL:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_CANCEL);
    case TOGGLE_PAUSE:
      if (download_item_ &&
          download_item_->GetState() == DownloadItem::IN_PROGRESS &&
          !download_item_->IsPaused())
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_PAUSE_ITEM);
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_RESUME_ITEM);
    case DISCARD:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_DISCARD);
    case KEEP:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_KEEP);
    case LEARN_MORE_SCANNING:
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_LEARN_MORE_SCANNING);
    case LEARN_MORE_INTERRUPTED:
      return l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_MENU_LEARN_MORE_INTERRUPTED);
  }
  NOTREACHED();
  return base::string16();
}

void DownloadShelfContextMenu::DetachFromDownloadItem() {
  if (!download_item_)
    return;

  download_item_->RemoveObserver(this);
  download_item_ = NULL;
}

void DownloadShelfContextMenu::OnDownloadDestroyed(DownloadItem* download) {
  DCHECK(download_item_ == download);
  DetachFromDownloadItem();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInProgressMenuModel() {
  if (in_progress_download_menu_model_)
    return in_progress_download_menu_model_.get();

  in_progress_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  in_progress_download_menu_model_->AddCheckItemWithStringId(
      OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN_WHEN_COMPLETE);
  in_progress_download_menu_model_->AddCheckItemWithStringId(
      ALWAYS_OPEN_TYPE, GetAlwaysOpenStringId());
  in_progress_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  in_progress_download_menu_model_->AddItemWithStringId(
      TOGGLE_PAUSE, IDS_DOWNLOAD_MENU_PAUSE_ITEM);
  in_progress_download_menu_model_->AddItemWithStringId(
      SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW);
  in_progress_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  in_progress_download_menu_model_->AddItemWithStringId(
      CANCEL, IDS_DOWNLOAD_MENU_CANCEL);

  return in_progress_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetFinishedMenuModel() {
  if (finished_download_menu_model_)
    return finished_download_menu_model_.get();

  finished_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  finished_download_menu_model_->AddItemWithStringId(
      OPEN_WHEN_COMPLETE, IDS_DOWNLOAD_MENU_OPEN);
  finished_download_menu_model_->AddCheckItemWithStringId(
      ALWAYS_OPEN_TYPE, GetAlwaysOpenStringId());
  finished_download_menu_model_->AddItemWithStringId(
      PLATFORM_OPEN, IDS_DOWNLOAD_MENU_PLATFORM_OPEN);
  finished_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  finished_download_menu_model_->AddItemWithStringId(
      SHOW_IN_FOLDER, IDS_DOWNLOAD_MENU_SHOW);
  finished_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  finished_download_menu_model_->AddItemWithStringId(
      CANCEL, IDS_DOWNLOAD_MENU_CANCEL);

  return finished_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetInterruptedMenuModel() {
#if !defined(OS_WIN)
  // If resumption isn't enabled and we aren't on Windows, then none of the
  // options here are applicable.
  if (!IsDownloadResumptionEnabled())
    return GetInProgressMenuModel();
#endif

  if (interrupted_download_menu_model_)
    return interrupted_download_menu_model_.get();

  interrupted_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  if (IsDownloadResumptionEnabled()) {
    interrupted_download_menu_model_->AddItemWithStringId(
        TOGGLE_PAUSE, IDS_DOWNLOAD_MENU_RESUME_ITEM);
  }
#if defined(OS_WIN)
  // The Help Center article is currently Windows specific.
  // TODO(asanka): Enable this for other platforms when the article is expanded
  // for other platforms.
  interrupted_download_menu_model_->AddItemWithStringId(
      LEARN_MORE_INTERRUPTED, IDS_DOWNLOAD_MENU_LEARN_MORE_INTERRUPTED);
#endif
  if (IsDownloadResumptionEnabled()) {
    interrupted_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    interrupted_download_menu_model_->AddItemWithStringId(
        CANCEL, IDS_DOWNLOAD_MENU_CANCEL);
  }

  return interrupted_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMaybeMaliciousMenuModel() {
  if (maybe_malicious_download_menu_model_)
    return maybe_malicious_download_menu_model_.get();

  maybe_malicious_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  maybe_malicious_download_menu_model_->AddItemWithStringId(
      KEEP, IDS_DOWNLOAD_MENU_KEEP);
  maybe_malicious_download_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  maybe_malicious_download_menu_model_->AddItemWithStringId(
      LEARN_MORE_SCANNING, IDS_DOWNLOAD_MENU_LEARN_MORE_SCANNING);
  return maybe_malicious_download_menu_model_.get();
}

ui::SimpleMenuModel* DownloadShelfContextMenu::GetMaliciousMenuModel() {
  if (malicious_download_menu_model_)
    return malicious_download_menu_model_.get();

  malicious_download_menu_model_.reset(new ui::SimpleMenuModel(this));

  DownloadItemModel download_model(download_item_);
  malicious_download_menu_model_->AddItemWithStringId(
      LEARN_MORE_SCANNING, IDS_DOWNLOAD_MENU_LEARN_MORE_SCANNING);

  return malicious_download_menu_model_.get();
}

int DownloadShelfContextMenu::GetAlwaysOpenStringId() const {
#if defined(OS_WIN)
  if (CanOpenPdfInReader())
    return IDS_DOWNLOAD_MENU_ALWAYS_OPEN_PDF_IN_READER;
#endif
  return IDS_DOWNLOAD_MENU_ALWAYS_OPEN_TYPE;
}

#if defined(OS_WIN)
bool DownloadShelfContextMenu::IsDownloadPdf() const {
  base::FilePath path = download_item_->GetTargetFilePath();
  return path.MatchesExtension(FILE_PATH_LITERAL(".pdf"));
}

bool DownloadShelfContextMenu::CanOpenPdfInReader() const {
  return (is_pdf_reader_up_to_date_ && IsDownloadPdf());
}
#endif
