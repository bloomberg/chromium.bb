// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugin_installer_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::DownloadItem;
using content::ResourceDispatcherHost;

namespace {

void BeginDownload(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_host_id,
    int render_view_host_routing_id,
    const ResourceDispatcherHost::DownloadStartedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  ResourceDispatcherHost* rdh = ResourceDispatcherHost::Get();
  scoped_ptr<net::URLRequest> request(new net::URLRequest(url, NULL));
  net::Error error = rdh->BeginDownload(
      request.Pass(),
      false,  // is_content_initiated
      resource_context,
      render_process_host_id,
      render_view_host_routing_id,
      true,  // prefer_cache
      content::DownloadSaveInfo(),
      callback);

  if (error != net::OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, content::DownloadId::Invalid(), error));
  }
}

}

PluginInstaller::PluginInstaller(const std::string& identifier,
                                 const GURL& plugin_url,
                                 const GURL& help_url,
                                 const string16& name,
                                 bool url_for_display,
                                 bool requires_authorization)
    : state_(kStateIdle),
      identifier_(identifier),
      plugin_url_(plugin_url),
      help_url_(help_url),
      name_(name),
      url_for_display_(url_for_display),
      requires_authorization_(requires_authorization) {
}

PluginInstaller::~PluginInstaller() {
}

void PluginInstaller::OnDownloadUpdated(DownloadItem* download) {
  DownloadItem::DownloadState state = download->GetState();
  switch (state) {
    case DownloadItem::IN_PROGRESS:
      return;
    case DownloadItem::COMPLETE: {
      DCHECK_EQ(kStateDownloading, state_);
      state_ = kStateIdle;
      FOR_EACH_OBSERVER(PluginInstallerObserver, observers_,
                        DownloadFinished());
      break;
    }
    case DownloadItem::CANCELLED: {
      DownloadCancelled();
      break;
    }
    case DownloadItem::REMOVING: {
      DCHECK_EQ(kStateDownloading, state_);
      state_ = kStateIdle;
      break;
    }
    case DownloadItem::INTERRUPTED: {
      content::DownloadInterruptReason reason = download->GetLastReason();
      DownloadError(content::InterruptReasonDebugString(reason));
      break;
    }
    case DownloadItem::MAX_DOWNLOAD_STATE: {
      NOTREACHED();
      return;
    }
  }
  download->RemoveObserver(this);
}

void PluginInstaller::OnDownloadOpened(DownloadItem* download) {
}

void PluginInstaller::AddObserver(PluginInstallerObserver* observer) {
  observers_.AddObserver(observer);
}

void PluginInstaller::RemoveObserver(PluginInstallerObserver* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.size() == weak_observers_.size()) {
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

void PluginInstaller::StartInstalling(TabContentsWrapper* wrapper) {
  DCHECK(state_ == kStateIdle);
  DCHECK(!url_for_display_);
  state_ = kStateDownloading;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadStarted());
  content::WebContents* web_contents = wrapper->web_contents();
  DownloadService* download_service =
      DownloadServiceFactory::GetForProfile(wrapper->profile());
  download_util::RecordDownloadSource(
      download_util::INITIATED_BY_PLUGIN_INSTALLER);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload,
                 plugin_url_,
                 wrapper->profile()->GetResourceContext(),
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetRenderViewHost()->GetRoutingID(),
                 base::Bind(&PluginInstaller::DownloadStarted,
                            base::Unretained(this),
                            make_scoped_refptr(
                                download_service->GetDownloadManager()))));
}

void PluginInstaller::DownloadStarted(
    scoped_refptr<content::DownloadManager> dlm,
    content::DownloadId download_id,
    net::Error error) {
  if (error != net::OK) {
    std::string msg =
        base::StringPrintf("Error %d: %s", error, net::ErrorToString(error));
    DownloadError(msg);
    return;
  }
  DownloadItem* download_item =
      dlm->GetActiveDownloadItem(download_id.local());
  download_item->SetOpenWhenComplete(true);
  download_item->AddObserver(this);
}

void PluginInstaller::OpenDownloadURL(content::WebContents* web_contents) {
  DCHECK(state_ == kStateIdle);
  DCHECK(url_for_display_);
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url_,
      content::Referrer(web_contents->GetURL(),
                        WebKit::WebReferrerPolicyDefault),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadFinished());
}

void PluginInstaller::DownloadError(const std::string& msg) {
  DCHECK(state_ == kStateDownloading);
  state_ = kStateIdle;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadError(msg));
}

void PluginInstaller::DownloadCancelled() {
  DCHECK(state_ == kStateDownloading);
  state_ = kStateIdle;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadCancelled());
}
