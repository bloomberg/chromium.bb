// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/web_contents.h"

using content::DownloadItem;

PluginInstaller::PluginInstaller()
    : state_(INSTALLER_STATE_IDLE),
      strong_observer_count_(0) {
}

PluginInstaller::~PluginInstaller() {
}

void PluginInstaller::OnDownloadUpdated(DownloadItem* download) {
  DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return;
    case DownloadItem::COMPLETE: {
      DCHECK_EQ(INSTALLER_STATE_DOWNLOADING, state_);
      state_ = INSTALLER_STATE_IDLE;
      FOR_EACH_OBSERVER(PluginInstallerObserver, observers_,
                        DownloadFinished());
      break;
    }
    case DownloadItem::CANCELLED: {
      DownloadCancelled();
      break;
    }
    case DownloadItem::INTERRUPTED: {
      content::DownloadInterruptReason reason = download->GetLastReason();
      DownloadError(content::DownloadInterruptReasonToString(reason));
      break;
    }
    case DownloadItem::MAX_DOWNLOAD_STATE: {
      NOTREACHED();
      return;
    }
  }
  download->RemoveObserver(this);
}

void PluginInstaller::OnDownloadDestroyed(DownloadItem* download) {
  DCHECK_EQ(INSTALLER_STATE_DOWNLOADING, state_);
  state_ = INSTALLER_STATE_IDLE;
  download->RemoveObserver(this);
}

void PluginInstaller::AddObserver(PluginInstallerObserver* observer) {
  strong_observer_count_++;
  observers_.AddObserver(observer);
}

void PluginInstaller::RemoveObserver(PluginInstallerObserver* observer) {
  strong_observer_count_--;
  observers_.RemoveObserver(observer);
  if (strong_observer_count_ == 0) {
    FOR_EACH_OBSERVER(WeakPluginInstallerObserver, weak_observers_,
                      OnlyWeakObserversLeft());
  }
}

void PluginInstaller::AddWeakObserver(WeakPluginInstallerObserver* observer) {
  weak_observers_.AddObserver(observer);
}

void PluginInstaller::RemoveWeakObserver(
    WeakPluginInstallerObserver* observer) {
  weak_observers_.RemoveObserver(observer);
}

void PluginInstaller::StartInstalling(const GURL& plugin_url,
                                      content::WebContents* web_contents) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext());
  StartInstallingWithDownloadManager(
      plugin_url, web_contents, download_manager);
}

void PluginInstaller::StartInstallingWithDownloadManager(
    const GURL& plugin_url,
    content::WebContents* web_contents,
    content::DownloadManager* download_manager) {
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  state_ = INSTALLER_STATE_DOWNLOADING;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadStarted());
  scoped_ptr<content::DownloadUrlParameters> download_parameters(
      content::DownloadUrlParameters::FromWebContents(web_contents,
                                                      plugin_url));
  download_parameters->set_callback(
      base::Bind(&PluginInstaller::DownloadStarted, base::Unretained(this)));
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PLUGIN_INSTALLER);
  download_manager->DownloadUrl(download_parameters.Pass());
}

void PluginInstaller::DownloadStarted(
    content::DownloadItem* item,
    content::DownloadInterruptReason interrupt_reason) {
  if (interrupt_reason != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
    std::string msg = base::StringPrintf(
        "Error %d: %s",
        interrupt_reason,
        content::DownloadInterruptReasonToString(interrupt_reason).c_str());
    DownloadError(msg);
    return;
  }
  item->SetOpenWhenComplete(true);
  item->AddObserver(this);
}

void PluginInstaller::OpenDownloadURL(const GURL& plugin_url,
                                      content::WebContents* web_contents) {
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url,
      content::Referrer(web_contents->GetURL(),
                        blink::WebReferrerPolicyDefault),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_TYPED, false));
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadFinished());
}

void PluginInstaller::DownloadError(const std::string& msg) {
  DCHECK_EQ(INSTALLER_STATE_DOWNLOADING, state_);
  state_ = INSTALLER_STATE_IDLE;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadError(msg));
}

void PluginInstaller::DownloadCancelled() {
  DCHECK_EQ(INSTALLER_STATE_DOWNLOADING, state_);
  state_ = INSTALLER_STATE_IDLE;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadCancelled());
}
