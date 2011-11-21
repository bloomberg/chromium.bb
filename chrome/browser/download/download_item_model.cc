// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/common/time_format.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/save_package.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

using base::TimeDelta;

// -----------------------------------------------------------------------------
// DownloadItemModel

DownloadItemModel::DownloadItemModel(DownloadItem* download)
    : BaseDownloadItemModel(download) {
}

void DownloadItemModel::CancelTask() {
  download_->Cancel(true /* update history service */);
}

string16 DownloadItemModel::GetStatusText() {
  int64 size = download_->GetReceivedBytes();
  int64 total = download_->GetTotalBytes();

  ui::DataUnits amount_units = ui::GetByteDisplayUnits(total);
  string16 simple_size = ui::FormatBytesWithUnits(size, amount_units, false);

  // In RTL locales, we render the text "size/total" in an RTL context. This
  // is problematic since a string such as "123/456 MB" is displayed
  // as "MB 123/456" because it ends with an LTR run. In order to solve this,
  // we mark the total string as an LTR string if the UI layout is
  // right-to-left so that the string "456 MB" is treated as an LTR run.
  string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
      ui::FormatBytesWithUnits(total, amount_units, true));

  TimeDelta remaining;
  string16 simple_time;
  if (download_->IsInProgress() && download_->IsPaused()) {
    simple_time = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  } else if (download_->TimeRemaining(&remaining)) {
    simple_time = download_->GetOpenWhenComplete() ?
                      TimeFormat::TimeRemainingShort(remaining) :
                      TimeFormat::TimeRemaining(remaining);
  }

  string16 status_text;
  switch (download_->GetState()) {
    case DownloadItem::IN_PROGRESS:
      if (ChromeDownloadManagerDelegate::IsExtensionDownload(download_) &&
          download_->AllDataSaved() &&
          download_->GetState() == DownloadItem::IN_PROGRESS) {
        // The download is a CRX (app, extension, theme, ...) and it is
        // being unpacked and validated.
        status_text = l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING);
      } else if (download_->GetOpenWhenComplete()) {
        if (simple_time.empty()) {
          status_text =
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE);
        } else {
          status_text = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_OPEN_IN,
                                                   simple_time);
        }
      } else {
        if (simple_time.empty()) {
          // Instead of displaying "0 B" we keep the "Starting..." string.
          status_text = (size == 0)
              ? l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING)
              : ui::FormatBytes(size);
        } else {
          status_text = l10n_util::GetStringFUTF16(
              IDS_DOWNLOAD_STATUS_IN_PROGRESS, simple_size, simple_total,
              simple_time);
        }
      }
      break;
    case DownloadItem::COMPLETE:
      if (download_->GetFileExternallyRemoved()) {
        status_text = l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_REMOVED);
      } else {
        status_text.clear();
      }
      break;
    case DownloadItem::CANCELLED:
      status_text = l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELED);
      break;
    case DownloadItem::REMOVING:
      break;
    case DownloadItem::INTERRUPTED:
      status_text = l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED,
                                               simple_size,
                                               simple_total);
      break;
    default:
      NOTREACHED();
  }

  return status_text;
}

// -----------------------------------------------------------------------------
// SavePageModel

SavePageModel::SavePageModel(SavePackage* save, DownloadItem* download)
    : BaseDownloadItemModel(download),
      save_(save) {
}

void SavePageModel::CancelTask() {
  save_->Cancel(true);
}

string16 SavePageModel::GetStatusText() {
  int64 size = download_->GetReceivedBytes();
  int64 total_size = download_->GetTotalBytes();

  string16 status_text;
  switch (download_->GetState()) {
    case DownloadItem::IN_PROGRESS:
      status_text = l10n_util::GetStringFUTF16(
          IDS_SAVE_PAGE_PROGRESS,
          base::FormatNumber(size),
          base::FormatNumber(total_size));
      break;
    case DownloadItem::COMPLETE:
      status_text = l10n_util::GetStringUTF16(IDS_SAVE_PAGE_STATUS_COMPLETED);
      break;
    case DownloadItem::CANCELLED:
      status_text = l10n_util::GetStringUTF16(IDS_SAVE_PAGE_STATUS_CANCELED);
      break;
    case DownloadItem::REMOVING:
      break;
    case DownloadItem::INTERRUPTED:
      status_text = l10n_util::GetStringFUTF16(
          IDS_SAVE_PAGE_STATUS_INTERRUPTED,
          base::FormatNumber(size),
          base::FormatNumber(total_size));
      break;
    default:
      NOTREACHED();
  }

  return status_text;
}
