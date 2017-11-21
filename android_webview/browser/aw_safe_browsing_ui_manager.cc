// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_ui_manager.h"

#include "android_webview/browser/aw_safe_browsing_blocking_page.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/common/aw_paths.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/base_ping_manager.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/safe_browsing_url_request_context_getter.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/safe_browsing/common/safebrowsing_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content::WebContents;

namespace {

std::string GetProtocolConfigClientName() {
  // Return a webview specific client name, see crbug.com/732373 for details.
  return "android_webview";
}

// UMA_HISTOGRAM_* macros expand to a lot of code, so wrap this in a helper.
void RecordIsWebViewViewable(bool isViewable) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.WebView.Viewable", isViewable);
}

}  // namespace

namespace android_webview {

AwSafeBrowsingUIManager::AwSafeBrowsingUIManager(
    AwURLRequestContextGetter* browser_url_request_context_getter,
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(timvolodine): verify this is what we want regarding the directory.
  base::FilePath user_data_dir;
  bool result =
      PathService::Get(android_webview::DIR_SAFE_BROWSING, &user_data_dir);
  DCHECK(result);

  url_request_context_getter_ =
      new safe_browsing::SafeBrowsingURLRequestContextGetter(
          browser_url_request_context_getter, user_data_dir);
}

AwSafeBrowsingUIManager::~AwSafeBrowsingUIManager() {}

void AwSafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = resource.web_contents_getter.Run();
  // Check the size of the view
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  if (!client || !client->CanShowInterstitial()) {
    RecordIsWebViewViewable(false);
    OnBlockingPageDone(std::vector<UnsafeResource>{resource}, false,
                       web_contents, resource.url.GetWithEmptyPath());
    return;
  }
  RecordIsWebViewViewable(true);
  safe_browsing::BaseUIManager::DisplayBlockingPage(resource);
}

void AwSafeBrowsingUIManager::ShowBlockingPageForResource(
    const UnsafeResource& resource) {
  AwSafeBrowsingBlockingPage::ShowBlockingPage(this, resource, pref_service_);
}

void AwSafeBrowsingUIManager::SetExtendedReportingAllowed(bool allowed) {
  pref_service_->SetBoolean(::prefs::kSafeBrowsingExtendedReportingOptInAllowed,
                            allowed);
}

int AwSafeBrowsingUIManager::GetErrorUiType(
    const UnsafeResource& resource) const {
  WebContents* web_contents = resource.web_contents_getter.Run();
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  DCHECK(client);
  return client->GetErrorUiType();
}

void AwSafeBrowsingUIManager::SendSerializedThreatDetails(
    const std::string& serialized) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!ping_manager_) {
    // Lazy creation of ping manager, needs to happen on IO thread.
    safe_browsing::SafeBrowsingProtocolConfig config;
    config.client_name = GetProtocolConfigClientName();
    base::CommandLine* cmdline = ::base::CommandLine::ForCurrentProcess();
    config.disable_auto_update =
        cmdline->HasSwitch(::safe_browsing::switches::kSbDisableAutoUpdate);
    config.url_prefix = ::safe_browsing::kSbDefaultURLPrefix;
    config.backup_connect_error_url_prefix =
        ::safe_browsing::kSbBackupConnectErrorURLPrefix;
    config.backup_http_error_url_prefix =
        ::safe_browsing::kSbBackupHttpErrorURLPrefix;
    config.backup_network_error_url_prefix =
        ::safe_browsing::kSbBackupNetworkErrorURLPrefix;
    ping_manager_ = ::safe_browsing::BasePingManager::Create(
        url_request_context_getter_.get(), config);
  }

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details";
    ping_manager_->ReportThreatDetails(serialized);
  }
}

}  // namespace android_webview
