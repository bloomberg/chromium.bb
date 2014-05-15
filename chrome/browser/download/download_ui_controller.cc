// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_controller.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/download_controller_android.h"
#else
#include "chrome/browser/profiles/profile.h"
#endif

namespace {

// DefaultUIControllerDelegate{Android,} is used when a DownloadUIController is
// constructed without specifying an explicit Delegate.
#if defined(OS_ANDROID)

class DefaultUIControllerDelegateAndroid
    : public DownloadUIController::Delegate {
 public:
  DefaultUIControllerDelegateAndroid() {}
  virtual ~DefaultUIControllerDelegateAndroid() {}

 private:
  // DownloadUIController::Delegate
  virtual void OnNewDownloadReady(content::DownloadItem* item) OVERRIDE;
};

void DefaultUIControllerDelegateAndroid::OnNewDownloadReady(
    content::DownloadItem* item) {
  // The Android DownloadController is only interested in IN_PROGRESS downloads.
  // Ones which are INTERRUPTED etc. can't be handed over to the Android
  // DownloadManager.
  if (item->GetState() != content::DownloadItem::IN_PROGRESS)
    return;

  // GET downloads without authentication are delegated to the Android
  // DownloadManager. Chrome is responsible for the rest.  See
  // InterceptDownloadResourceThrottle::ProcessDownloadRequest().
  content::DownloadControllerAndroid::Get()->OnDownloadStarted(item);
}

#else  // OS_ANDROID

class DefaultUIControllerDelegate : public DownloadUIController::Delegate {
 public:
  // |profile| is required to outlive DefaultUIControllerDelegate.
  explicit DefaultUIControllerDelegate(Profile* profile)
      : profile_(profile) {}
  virtual ~DefaultUIControllerDelegate() {}

 private:
  // DownloadUIController::Delegate
  virtual void OnNewDownloadReady(content::DownloadItem* item) OVERRIDE;

  Profile* profile_;
};

void DefaultUIControllerDelegate::OnNewDownloadReady(
    content::DownloadItem* item) {
  content::WebContents* web_contents = item->GetWebContents();
  Browser* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : NULL;

  // As a last resort, use the last active browser for this profile. Not ideal,
  // but better than not showing the download at all.
  if (browser == NULL) {
    browser = chrome::FindLastActiveWithProfile(profile_,
                                                chrome::GetActiveDesktop());
  }

  if (browser)
    browser->ShowDownload(item);
}

#endif  // !OS_ANDROID

} // namespace

DownloadUIController::Delegate::~Delegate() {
}

DownloadUIController::DownloadUIController(content::DownloadManager* manager,
                                           scoped_ptr<Delegate> delegate)
    : download_notifier_(manager, this),
      delegate_(delegate.Pass()) {
  if (!delegate_) {
#if defined(OS_ANDROID)
    delegate_.reset(new DefaultUIControllerDelegateAndroid());
#else
    // The delegate should not be invoked after the profile has gone away. This
    // should be the case since DownloadUIController is owned by
    // DownloadService, which in turn is a profile keyed service.
    delegate_.reset(new DefaultUIControllerDelegate(
        Profile::FromBrowserContext(manager->GetBrowserContext())));
#endif
  }
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

  // Wait until the target path is determined.
  if (item->GetTargetFilePath().empty())
    return;

  DownloadItemModel(item).SetWasUINotified(true);
  delegate_->OnNewDownloadReady(item);
}
