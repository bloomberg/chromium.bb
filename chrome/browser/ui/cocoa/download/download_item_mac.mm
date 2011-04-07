// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/download/download_item_mac.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#import "chrome/browser/ui/cocoa/download/download_item_controller.h"
#include "chrome/browser/ui/cocoa/download/download_util_mac.h"
#include "ui/gfx/image.h"

// DownloadItemMac -------------------------------------------------------------

DownloadItemMac::DownloadItemMac(BaseDownloadItemModel* download_model,
                                 DownloadItemController* controller)
    : download_model_(download_model), item_controller_(controller) {
  download_model_->download()->AddObserver(this);
}

DownloadItemMac::~DownloadItemMac() {
  download_model_->download()->RemoveObserver(this);
  icon_consumer_.CancelAllRequests();
}

void DownloadItemMac::OnDownloadUpdated(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());

  if ([item_controller_ isDangerousMode] &&
      download->safety_state() == DownloadItem::DANGEROUS_BUT_VALIDATED) {
    // We have been approved.
    [item_controller_ clearDangerousMode];
  }

  if (download->GetUserVerifiedFilePath() != lastFilePath_) {
    // Turns out the file path is "Unconfirmed %d.crdownload" for dangerous
    // downloads. When the download is confirmed, the file is renamed on
    // another thread, so reload the icon if the download filename changes.
    LoadIcon();
    lastFilePath_ = download->GetUserVerifiedFilePath();

    [item_controller_ updateToolTip];
  }

  switch (download->state()) {
    case DownloadItem::REMOVING:
      [item_controller_ remove];  // We're deleted now!
      break;
    case DownloadItem::COMPLETE:
      if (download->auto_opened()) {
        [item_controller_ remove];  // We're deleted now!
        return;
      }
      download_util::NotifySystemOfDownloadComplete(download->full_path());
      // fall through
    case DownloadItem::IN_PROGRESS:
    case DownloadItem::INTERRUPTED:
    case DownloadItem::CANCELLED:
      [item_controller_ setStateFromDownload:download_model_.get()];
      break;
    default:
      NOTREACHED();
  }
}

void DownloadItemMac::OnDownloadOpened(DownloadItem* download) {
  DCHECK_EQ(download, download_model_->download());
  [item_controller_ downloadWasOpened];
}

void DownloadItemMac::LoadIcon() {
  IconManager* icon_manager = g_browser_process->icon_manager();
  if (!icon_manager) {
    NOTREACHED();
    return;
  }

  // We may already have this particular image cached.
  FilePath file = download_model_->download()->GetUserVerifiedFilePath();
  gfx::Image* icon = icon_manager->LookupIcon(file, IconLoader::SMALL);
  if (icon) {
    [item_controller_ setIcon:*icon];
    return;
  }

  // The icon isn't cached, load it asynchronously.
  icon_manager->LoadIcon(file, IconLoader::SMALL, &icon_consumer_,
                         NewCallback(this,
                                     &DownloadItemMac::OnExtractIconComplete));
}

void DownloadItemMac::OnExtractIconComplete(IconManager::Handle handle,
                                            gfx::Image* icon) {
  if (!icon)
    return;
  [item_controller_ setIcon:*icon];
}
