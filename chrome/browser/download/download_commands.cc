// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_commands.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/mime_util/mime_util.h"
#include "grit/theme_resources.h"
#include "net/base/url_util.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

namespace {

// Maximum size (compressed) of image to be copied to the clipboard. If the
// image exceeds this size, the image is not copied.
const int64_t kMaxImageClipboardSize = 20 * 1024 * 1024;  // 20 MB

class ImageClipboardCopyManager : public ImageDecoder::ImageRequest {
 public:
  static void Start(const base::FilePath& file_path) {
    new ImageClipboardCopyManager(file_path);
  }

 private:
  explicit ImageClipboardCopyManager(const base::FilePath& file_path)
      : file_path_(file_path) {
    // Constructor must be called in the UI thread.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(&ImageClipboardCopyManager::StartDecoding,
                   base::Unretained(this)));
  }

  void StartDecoding() {
    DCHECK(content::BrowserThread::GetBlockingPool()->
        RunsTasksOnCurrentThread());

    // Re-check the filesize since the file may be modified after downloaded.
    int64_t filesize;
    if (!GetFileSize(file_path_, &filesize) ||
        filesize > kMaxImageClipboardSize) {
      OnFailedBeforeDecoding();
      return;
    }

    std::string data;
    bool ret = base::ReadFileToString(file_path_, &data);
    if (!ret || data.empty()) {
      OnFailedBeforeDecoding();
      return;
    }

    // Note: An image over 128MB (uncompressed) may fail, due to the limitation
    // of IPC message size.
    ImageDecoder::Start(this, data);
  }

  void OnImageDecoded(const SkBitmap& decoded_image) override {
    // This method is called on the same thread as constructor (the UI thread).
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
    scw.Reset();

    if (!decoded_image.empty() && !decoded_image.isNull())
      scw.WriteImage(decoded_image);

    delete this;
  }

  void OnDecodeImageFailed() override {
    // This method is called on the same thread as constructor (the UI thread).
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    delete this;
  }

  void OnFailedBeforeDecoding() {
    // We don't need to cancel the job, since it shouldn't be started here.

    task_runner()->DeleteSoon(FROM_HERE, this);
  }

  const base::FilePath file_path_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImageClipboardCopyManager);
};

}  // anonymous namespace

DownloadCommands::DownloadCommands(content::DownloadItem* download_item)
    : download_item_(download_item) {
  DCHECK(download_item);
}

int DownloadCommands::GetCommandIconId(Command command) const {
  switch (command) {
    case PAUSE:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_PAUSE;
    case RESUME:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_DOWNLOAD;
    case SHOW_IN_FOLDER:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_FOLDER;
    case KEEP:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_DOWNLOAD;
    case DISCARD:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_DELETE;
    case CANCEL:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_CANCEL;
    case LEARN_MORE_SCANNING:
      return IDR_NOTIFICATION_WELCOME_LEARN_MORE;
    case COPY_TO_CLIPBOARD:
      return IDR_DOWNLOAD_NOTIFICATION_MENU_COPY_TO_CLIPBOARD;
    case OPEN_WHEN_COMPLETE:
    case ALWAYS_OPEN_TYPE:
    case PLATFORM_OPEN:
    case LEARN_MORE_INTERRUPTED:
      return -1;
  }
  NOTREACHED();
  return -1;
}

GURL DownloadCommands::GetLearnMoreURLForInterruptedDownload() const {
  GURL learn_more_url(chrome::kDownloadInterruptedLearnMoreURL);
  learn_more_url = google_util::AppendGoogleLocaleParam(
      learn_more_url, g_browser_process->GetApplicationLocale());
  return net::AppendQueryParameter(
      learn_more_url, "ctx",
      base::IntToString(static_cast<int>(download_item_->GetLastReason())));
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
             download_util::IsAllowedToOpenAutomatically(
                 download_item_->GetTargetFilePath()) &&
             !download_crx_util::IsExtensionDownload(*download_item_);
    case CANCEL:
      return !download_item_->IsDone();
    case PAUSE:
      return !download_item_->IsDone() && !download_item_->IsPaused() &&
             download_item_->GetState() == content::DownloadItem::IN_PROGRESS;
    case RESUME:
      return download_item_->CanResume() &&
             (download_item_->IsPaused() ||
              download_item_->GetState() != content::DownloadItem::IN_PROGRESS);
    case COPY_TO_CLIPBOARD:
      return (download_item_->GetState() == content::DownloadItem::COMPLETE &&
              download_item_->GetReceivedBytes() <= kMaxImageClipboardSize);
    case DISCARD:
    case KEEP:
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
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
    case LEARN_MORE_SCANNING:
    case LEARN_MORE_INTERRUPTED:
    case COPY_TO_CLIPBOARD:
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

      safe_browsing::SafeBrowsingService* sb_service =
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
          GetLearnMoreURLForInterruptedDownload(), content::Referrer(),
          NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
      break;
    case PAUSE:
      download_item_->Pause();
      break;
    case RESUME:
      download_item_->Resume();
      break;
    case COPY_TO_CLIPBOARD:
      CopyFileAsImageToClipboard();
      break;
  }
}

Browser* DownloadCommands::GetBrowser() const {
  Profile* profile =
      Profile::FromBrowserContext(download_item_->GetBrowserContext());
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile);
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

void DownloadCommands::CopyFileAsImageToClipboard() const {
  if (download_item_->GetState() != content::DownloadItem::COMPLETE ||
      download_item_->GetReceivedBytes() > kMaxImageClipboardSize) {
      return;
  }

  // TODO(yoshiki): Refine the code by combining the common logic with the
  // preview in DownloadItemNotification.
  std::string mime = download_item_->GetMimeType();
  if (!mime_util::IsSupportedImageMimeType(mime)) {
    base::FilePath::StringType extension_with_dot =
        download_item_->GetTargetFilePath().FinalExtension();
    if (extension_with_dot.empty() ||
        !net::GetWellKnownMimeTypeFromExtension(extension_with_dot.substr(1),
                                                &mime) ||
        !mime_util::IsSupportedImageMimeType(mime)) {
      // It seems a non-image file.
      return;
    }
  }

  base::FilePath file_path = download_item_->GetFullPath();
  ImageClipboardCopyManager::Start(file_path);
}
