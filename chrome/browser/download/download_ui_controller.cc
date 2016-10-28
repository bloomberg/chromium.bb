// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_controller.h"

#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_controller_base.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/download/notification/download_notification_manager.h"
#endif

namespace {

// DownloadShelfUIControllerDelegate{Android,} is used when a
// DownloadUIController is
// constructed without specifying an explicit Delegate.
#if defined(OS_ANDROID)

class AndroidUIControllerDelegate : public DownloadUIController::Delegate {
 public:
  AndroidUIControllerDelegate() {}
  ~AndroidUIControllerDelegate() override {}

 private:
  // DownloadUIController::Delegate
  void OnNewDownloadReady(content::DownloadItem* item) override;
};

void AndroidUIControllerDelegate::OnNewDownloadReady(
    content::DownloadItem* item) {
  // The Android DownloadController is only interested in IN_PROGRESS downloads.
  // Ones which are INTERRUPTED etc. can't be handed over to the Android
  // DownloadManager.
  if (item->GetState() != content::DownloadItem::IN_PROGRESS)
    return;

  DownloadControllerBase::Get()->OnDownloadStarted(item);
}

#else  // OS_ANDROID

class DownloadShelfUIControllerDelegate
    : public DownloadUIController::Delegate {
 public:
  // |profile| is required to outlive DownloadShelfUIControllerDelegate.
  explicit DownloadShelfUIControllerDelegate(Profile* profile)
      : profile_(profile) {}
  ~DownloadShelfUIControllerDelegate() override {}

 private:
  // DownloadUIController::Delegate
  void OnNewDownloadReady(content::DownloadItem* item) override;

  Profile* profile_;
};

void DownloadShelfUIControllerDelegate::OnNewDownloadReady(
    content::DownloadItem* item) {
  content::WebContents* web_contents = item->GetWebContents();
  // For the case of DevTools web contents, we'd like to use target browser
  // shelf although saving from the DevTools web contents.
  if (web_contents && DevToolsWindow::IsDevToolsWindow(web_contents)) {
    DevToolsWindow* devtools_window =
        DevToolsWindow::AsDevToolsWindow(web_contents);
    content::WebContents* inspected =
        devtools_window->GetInspectedWebContents();
    // Do not overwrite web contents for the case of remote debugging.
    if (inspected)
      web_contents = inspected;
  }
  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : NULL;

  // As a last resort, use the last active browser for this profile. Not ideal,
  // but better than not showing the download at all.
  if (browser == nullptr)
    browser = chrome::FindLastActiveWithProfile(profile_);

  if (browser && browser->window() &&
      DownloadItemModel(item).ShouldShowInShelf()) {
    // GetDownloadShelf creates the download shelf if it was not yet created.
    browser->window()->GetDownloadShelf()->AddDownload(item);
  }
}

#endif  // !OS_ANDROID

} // namespace

DownloadUIController::Delegate::~Delegate() {
}

DownloadUIController::DownloadUIController(content::DownloadManager* manager,
                                           std::unique_ptr<Delegate> delegate)
    : download_notifier_(manager, this), delegate_(std::move(delegate)) {
#if defined(OS_ANDROID)
  if (!delegate_)
    delegate_.reset(new AndroidUIControllerDelegate());
#elif defined(OS_CHROMEOS)
  if (!delegate_) {
    // The Profile is guaranteed to be valid since DownloadUIController is owned
    // by DownloadService, which in turn is a profile keyed service.
    delegate_.reset(new DownloadNotificationManager(
        Profile::FromBrowserContext(manager->GetBrowserContext())));
  }
#else  // defined(OS_CHROMEOS)
  if (!delegate_) {
    delegate_.reset(new DownloadShelfUIControllerDelegate(
        Profile::FromBrowserContext(manager->GetBrowserContext())));
  }
#endif  // defined(OS_ANDROID)
}

DownloadUIController::~DownloadUIController() {
}

void DownloadUIController::OnDownloadCreated(content::DownloadManager* manager,
                                             content::DownloadItem* item) {
  // SavePackage downloads are created in a state where they can be shown in the
  // browser. Call OnDownloadUpdated() once to notify the UI immediately.
  OnDownloadUpdated(manager, item);
}

void DownloadUIController::OnDownloadUpdated(content::DownloadManager* manager,
                                             content::DownloadItem* item) {
  DownloadItemModel item_model(item);

  // Ignore if we've already notified the UI about |item| or if it isn't a new
  // download.
  if (item_model.WasUINotified() || !item_model.ShouldNotifyUI())
    return;

  // Wait until the target path is determined or the download is canceled.
  if (item->GetTargetFilePath().empty() &&
      item->GetState() != content::DownloadItem::CANCELLED)
    return;

#if !defined(OS_ANDROID)
  content::WebContents* web_contents = item->GetWebContents();
  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    // If the download occurs in a new tab, and it's not a save page
    // download (started before initial navigation completed) close it.
    // Avoid calling CloseContents if the tab is not in this browser's tab strip
    // model; this can happen if the download was initiated by something
    // internal to Chrome, such as by the app list.
    if (browser && web_contents->GetController().IsInitialNavigation() &&
        browser->tab_strip_model()->count() > 1 &&
        browser->tab_strip_model()->GetIndexOfWebContents(web_contents) !=
            TabStripModel::kNoTab &&
        !item->IsSavePackageDownload()) {
      web_contents->Close();
    }
  }
#endif

  if (item->GetState() == content::DownloadItem::CANCELLED)
    return;

  DownloadItemModel(item).SetWasUINotified(true);
  delegate_->OnNewDownloadReady(item);
}
