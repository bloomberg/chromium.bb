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
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/browser_context.h"
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
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_utils.h"
#include "webkit/plugins/webplugininfo.h"

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
      content::DownloadSaveInfo(),
      callback);

  if (error != net::OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, content::DownloadId::Invalid(), error));
  }
}

}  // namespace

PluginInstaller::PluginInstaller(const std::string& identifier,
                                 const string16& name,
                                 bool url_for_display,
                                 const GURL& plugin_url,
                                 const GURL& help_url,
                                 const string16& group_name_matcher)
    : identifier_(identifier),
      name_(name),
      group_name_matcher_(group_name_matcher),
      url_for_display_(url_for_display),
      plugin_url_(plugin_url),
      help_url_(help_url),
      state_(INSTALLER_STATE_IDLE) {
}

PluginInstaller::~PluginInstaller() {
}

void PluginInstaller::AddVersion(const Version& version,
                                 SecurityStatus status) {
  DCHECK(versions_.find(version) == versions_.end());
  versions_[version] = status;
}

PluginInstaller::SecurityStatus PluginInstaller::GetSecurityStatus(
    const webkit::WebPluginInfo& plugin) const {
  // If there are no versions defined, the plug-in should require authorization.
  if (versions_.empty())
    return SECURITY_STATUS_REQUIRES_AUTHORIZATION;

  Version version;
  webkit::npapi::CreateVersionFromString(plugin.version, &version);
  if (!version.IsValid())
    version = Version("0");

  // |lower_bound| returns the latest version that is not newer than |version|.
  std::map<Version, SecurityStatus, VersionComparator>::const_iterator it =
      versions_.lower_bound(version);
  // If there is at least one version defined, everything older than the oldest
  // defined version is considered out-of-date.
  if (it == versions_.end())
    return SECURITY_STATUS_OUT_OF_DATE;

  return it->second;
}

bool PluginInstaller::VersionComparator::operator() (const Version& lhs,
                                                     const Version& rhs) const {
  // Keep versions ordered by newest (biggest) first.
  return lhs.CompareTo(rhs) > 0;
}

// static
bool PluginInstaller::ParseSecurityStatus(
    const std::string& status_str,
    PluginInstaller::SecurityStatus* status) {
  if (status_str == "up_to_date")
    *status = SECURITY_STATUS_UP_TO_DATE;
  else if (status_str == "out_of_date")
    *status = SECURITY_STATUS_OUT_OF_DATE;
  else if (status_str == "requires_authorization")
    *status = SECURITY_STATUS_REQUIRES_AUTHORIZATION;
  else
    return false;

  return true;
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

void PluginInstaller::StartInstalling(TabContents* tab_contents) {
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  DCHECK(!url_for_display_);
  state_ = INSTALLER_STATE_DOWNLOADING;
  FOR_EACH_OBSERVER(PluginInstallerObserver, observers_, DownloadStarted());
  content::WebContents* web_contents = tab_contents->web_contents();
  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(tab_contents->profile());
  download_util::RecordDownloadSource(
      download_util::INITIATED_BY_PLUGIN_INSTALLER);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload,
                 plugin_url_,
                 tab_contents->profile()->GetResourceContext(),
                 web_contents->GetRenderProcessHost()->GetID(),
                 web_contents->GetRenderViewHost()->GetRoutingID(),
                 base::Bind(&PluginInstaller::DownloadStarted,
                            base::Unretained(this),
                            make_scoped_refptr(download_manager))));
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
  DCHECK_EQ(INSTALLER_STATE_IDLE, state_);
  DCHECK(url_for_display_);
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url_,
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

bool PluginInstaller::MatchesPlugin(const webkit::WebPluginInfo& plugin) {
  return plugin.name.find(group_name_matcher_) != string16::npos;
}
