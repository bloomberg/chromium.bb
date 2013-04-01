// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/animation/animation.h"

using content::DownloadItem;

namespace {

// Delay before we show a transient download.
const int64 kDownloadShowDelayInSeconds = 2;

} // namespace

DownloadShelf::DownloadShelf()
    : should_show_on_unhide_(false),
      is_hidden_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

DownloadShelf::~DownloadShelf() {
}

void DownloadShelf::AddDownload(DownloadItem* download) {
  DCHECK(download);
  if (DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete()) {
    // If we are going to remove the download from the shelf upon completion,
    // wait a few seconds to see if it completes quickly. If it's a small
    // download, then the user won't have time to interact with it.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&DownloadShelf::ShowDownloadById,
                   weak_ptr_factory_.GetWeakPtr(),
                   download->GetId()),
        GetTransientDownloadShowDelay());
  } else {
    ShowDownload(download);
  }
}

void DownloadShelf::Show() {
  if (is_hidden_) {
    should_show_on_unhide_ = true;
    return;
  }
  DoShow();
}

void DownloadShelf::Close(CloseReason reason) {
  if (is_hidden_) {
    should_show_on_unhide_ = false;
    return;
  }
  DoClose(reason);
}

void DownloadShelf::Hide() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  if (IsShowing()) {
    should_show_on_unhide_ = true;
    DoClose(AUTOMATIC);
  }
}

void DownloadShelf::Unhide() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  if (should_show_on_unhide_) {
    should_show_on_unhide_ = false;
    DoShow();
  }
}

base::TimeDelta DownloadShelf::GetTransientDownloadShowDelay() {
  return base::TimeDelta::FromSeconds(kDownloadShowDelayInSeconds);
}

content::DownloadManager* DownloadShelf::GetDownloadManager() {
  DCHECK(browser());
  return content::BrowserContext::GetDownloadManager(browser()->profile());
}

void DownloadShelf::ShowDownload(DownloadItem* download) {
  if (download->IsComplete() &&
      DownloadItemModel(download).ShouldRemoveFromShelfWhenComplete()) {
    return;
  }

  if (is_hidden_)
    Unhide();
  Show();
  DoAddDownload(download);

  // browser() can be NULL for tests.
  if (!browser())
    return;

  // Show the download started animation if:
  // - Download started animation is enabled for this download. It is disabled
  //   for "Save As" downloads and extension installs, for example.
  // - The browser has an active visible WebContents. (browser isn't minimized,
  //   or running under a test etc.)
  // - Rich animations are enabled.
  content::WebContents* shelf_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  if (DownloadItemModel(download).ShouldShowDownloadStartedAnimation() &&
      shelf_tab &&
      platform_util::IsVisible(shelf_tab->GetView()->GetNativeView()) &&
      ui::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(shelf_tab);
  }
}

void DownloadShelf::ShowDownloadById(int32 download_id) {
  content::DownloadManager* download_manager = GetDownloadManager();
  if (!download_manager)
    return;

  DownloadItem* download = download_manager->GetDownload(download_id);
  if (!download)
    return;

  ShowDownload(download);
}
