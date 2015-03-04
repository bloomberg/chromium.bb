// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_commands.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

DownloadCommands::DownloadCommands(content::DownloadItem* download_item)
    : download_item_(download_item) {
  DCHECK(download_item);
}

int DownloadCommands::GetCommandIconId(Command command) {
  switch (command) {
    case PAUSE:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_PAUSE;
    case RESUME:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_RESUME;
    case SHOW_IN_FOLDER:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_FOLDER;
    case RETRY:
    case KEEP:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_DOWNLOAD;
    case DISCARD:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_DELETE;
    case OPEN_WHEN_COMPLETE:
    case ALWAYS_OPEN_TYPE:
    case PLATFORM_OPEN:
    case CANCEL:
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
      return -1;
  }
  NOTREACHED();
  return -1;
}

gfx::Image DownloadCommands::GetCommandIcon(Command command) {
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  return bundle.GetImageNamed(GetCommandIconId(command));
}

bool DownloadCommands::IsCommandEnabled(Command command) const {
  switch (command) {
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
    case PAUSE:
      return !download_item_->IsDone() && !download_item_->IsPaused() &&
             download_item_->GetState() == content::DownloadItem::IN_PROGRESS;
    case RESUME:
      return !download_item_->CanResume() &&
             (download_item_->IsPaused() ||
              download_item_->GetState() != content::DownloadItem::IN_PROGRESS);
    case DISCARD:
    case KEEP:
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
    case RETRY:
      return true;
  }
  NOTREACHED();
  return false;
}

bool DownloadCommands::IsCommandChecked(Command command) const {
  switch (command) {
    case OPEN_WHEN_COMPLETE:
      return download_item_->GetOpenWhenComplete() ||
             download_crx_util::IsExtensionDownload(*download_item_);
    case ALWAYS_OPEN_TYPE:
#if defined(OS_WIN) || defined(OS_LINUX) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
      if (CanOpenPdfInSystemViewer()) {
        DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
            download_item_->GetBrowserContext());
        return prefs->ShouldOpenPdfInSystemReader();
      }
#endif
      return download_item_->ShouldOpenFileBasedOnExtension();
    case PAUSE:
    case RESUME:
      return download_item_->IsPaused();
    case SHOW_IN_FOLDER:
    case PLATFORM_OPEN:
    case CANCEL:
    case DISCARD:
    case KEEP:
    case RETRY:
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
      return false;
  }
  return false;
}

bool DownloadCommands::IsCommandVisible(Command command) const {
  if (command == PLATFORM_OPEN)
    return (DownloadItemModel(download_item_).ShouldPreferOpeningInBrowser());

  return true;
}

void DownloadCommands::ExecuteCommand(Command command) {
  switch (command) {
    case SHOW_IN_FOLDER:
      download_item_->ShowDownloadInShell();
      break;
    case OPEN_WHEN_COMPLETE:
      download_item_->OpenDownload();
      break;
    case ALWAYS_OPEN_TYPE: {
      bool is_checked = IsCommandChecked(ALWAYS_OPEN_TYPE);
      DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
          download_item_->GetBrowserContext());
#if defined(OS_WIN) || defined(OS_LINUX) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
      if (CanOpenPdfInSystemViewer()) {
        prefs->SetShouldOpenPdfInSystemReader(!is_checked);
        DownloadItemModel(download_item_)
            .SetShouldPreferOpeningInBrowser(is_checked);
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
          (sb_service ? sb_service->download_protection_service() : nullptr);
      if (protection_service)
        protection_service->ShowDetailsForDownload(*download_item_,
                                                   GetBrowser());
#else
      // Should only be getting invoked if we are using safe browsing.
      NOTREACHED();
#endif
      break;
    }
    case LEARN_MORE_INTERRUPTED:
      GetBrowser()->OpenURL(content::OpenURLParams(
          GURL(chrome::kDownloadInterruptedLearnMoreURL), content::Referrer(),
          NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
      break;
    case PAUSE:
      download_item_->Pause();
      break;
    case RESUME:
      download_item_->Resume();
      break;
    case RETRY:
      if (download_item_->CanResume()) {
        download_item_->Resume();
      } else {
        // TODO(yoshiki): Implement retry logic.
      }
      break;
  }
}

Browser* DownloadCommands::GetBrowser() const {
  Profile* profile =
      Profile::FromBrowserContext(download_item_->GetBrowserContext());
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      profile, chrome::GetActiveDesktop());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
bool DownloadCommands::IsDownloadPdf() const {
  base::FilePath path = download_item_->GetTargetFilePath();
  return path.MatchesExtension(FILE_PATH_LITERAL(".pdf"));
}
#endif

bool DownloadCommands::CanOpenPdfInSystemViewer() const {
#if defined(OS_WIN)
  bool is_adobe_pdf_reader_up_to_date = false;
  if (IsDownloadPdf() && IsAdobeReaderDefaultPDFViewer()) {
    is_adobe_pdf_reader_up_to_date =
        DownloadTargetDeterminer::IsAdobeReaderUpToDate();
  }
  return IsDownloadPdf() &&
         (IsAdobeReaderDefaultPDFViewer() ? is_adobe_pdf_reader_up_to_date
                                          : true);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  return IsDownloadPdf();
#endif
}
