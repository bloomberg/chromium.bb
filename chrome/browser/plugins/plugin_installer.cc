// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
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
  scoped_ptr<net::URLRequest> request(
      resource_context->GetRequestContext()->CreateRequest(url, NULL));
  net::Error error = rdh->BeginDownload(
      request.Pass(),
      false,  // is_content_initiated
      resource_context,
      render_process_host_id,
      render_view_host_routing_id,
      true,  // prefer_cache
      scoped_ptr<content::DownloadSaveInfo>(new content::DownloadSaveInfo()),
      callback);

  if (error != net::OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, static_cast<DownloadItem*>(NULL), error));
  }
}

}  // namespace

PluginInstaller::PluginInstaller()
    : state_(INSTALLER_STATE_IDLE) {
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

void PluginInstaller::OnDownloadDestroyed(DownloadItem* download) {
  DCHECK_EQ(INSTALLER_STATE_DOWNLOADING, state_);
  state_ = INSTALLER_STATE_IDLE;
  download->RemoveObserver(this);
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

void PluginInstaller::StartInstalling(const GURL& plugin_url,
                                      content::WebContents* web_contents) {
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  state_ = INSTALLER_STATE_DOWNLOADING;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadStarted());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(profile);
  download_util::RecordDownloadSource(
      download_util::INITIATED_BY_PLUGIN_INSTALLER);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload,
                 plugin_url,
                 profile->GetResourceContext(),
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetRenderViewHost()->GetRoutingID(),
                 base::Bind(&PluginInstaller::DownloadStarted,
                            base::Unretained(this),
                            make_scoped_refptr(download_manager))));
}

void PluginInstaller::DownloadStarted(
    scoped_refptr<content::DownloadManager> dlm,
    content::DownloadItem* item,
    net::Error error) {
  if (!item) {
    DCHECK_NE(net::OK, error);
    std::string msg =
        base::StringPrintf("Error %d: %s", error, net::ErrorToString(error));
    DownloadError(msg);
    return;
  }
  DCHECK_EQ(net::OK, error);
  item->SetOpenWhenComplete(true);
  item->AddObserver(this);
}

void PluginInstaller::OpenDownloadURL(const GURL& plugin_url,
                                      content::WebContents* web_contents) {
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url,
      content::Referrer(web_contents->GetURL(),
                        WebKit::WebReferrerPolicyDefault),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED, false));
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
