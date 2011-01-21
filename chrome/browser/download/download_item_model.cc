// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_model.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
  int64 size = download_->received_bytes();
  int64 total = download_->total_bytes();

  DataUnits amount_units = GetByteDisplayUnits(total);
  const string16 simple_size = FormatBytes(size, amount_units, false);

  // In RTL locales, we render the text "size/total" in an RTL context. This
  // is problematic since a string such as "123/456 MB" is displayed
  // as "MB 123/456" because it ends with an LTR run. In order to solve this,
  // we mark the total string as an LTR string if the UI layout is
  // right-to-left so that the string "456 MB" is treated as an LTR run.
  string16 simple_total = base::i18n::GetDisplayStringInLTRDirectionality(
      FormatBytes(total, amount_units, true));

  TimeDelta remaining;
  string16 simple_time;
  if (download_->state() == DownloadItem::IN_PROGRESS &&
      download_->is_paused()) {
    simple_time = l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);
  } else if (download_->TimeRemaining(&remaining)) {
    simple_time = download_->open_when_complete() ?
                      TimeFormat::TimeRemainingShort(remaining) :
                      TimeFormat::TimeRemaining(remaining);
  }

  string16 status_text;
  switch (download_->state()) {
    case DownloadItem::IN_PROGRESS:
      if (download_->open_when_complete()) {
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
          status_text = (size == 0) ?
              l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING) :
              FormatBytes(size, GetByteDisplayUnits(size), true);
        } else {
          status_text = l10n_util::GetStringFUTF16(
              IDS_DOWNLOAD_STATUS_IN_PROGRESS, simple_size, simple_total,
              simple_time);
        }
      }
      break;
    case DownloadItem::COMPLETE:
      status_text.clear();
      break;
    case DownloadItem::CANCELLED:
      status_text = l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELED);
      break;
    case DownloadItem::REMOVING:
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
  int64 size = download_->received_bytes();
  int64 total_size = download_->total_bytes();

  string16 status_text;
  switch (download_->state()) {
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
    default:
      NOTREACHED();
  }

  return status_text;
}
