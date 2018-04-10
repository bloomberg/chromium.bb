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
#include "components/safe_browsing/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/browser/safe_browsing_url_request_context_getter.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
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

  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          url_request_context_getter_);
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

scoped_refptr<network::SharedURLLoaderFactory>
AwSafeBrowsingUIManager::GetURLLoaderFactoryOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!shared_url_loader_factory_on_io_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&AwSafeBrowsingUIManager::CreateURLLoaderFactoryForIO,
                       this, MakeRequest(&url_loader_factory_on_io_)));
    shared_url_loader_factory_on_io_ =
        base::MakeRefCounted<content::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_on_io_.get());
  }
  return shared_url_loader_factory_on_io_;
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ping_manager_) {
    // Lazy creation of ping manager, needs to happen on IO thread.
    safe_browsing::SafeBrowsingProtocolConfig config;
    config.client_name = GetProtocolConfigClientName();
    config.disable_auto_update = false;
    config.url_prefix = ::safe_browsing::kSbDefaultURLPrefix;
    config.backup_connect_error_url_prefix =
        ::safe_browsing::kSbBackupConnectErrorURLPrefix;
    config.backup_http_error_url_prefix =
        ::safe_browsing::kSbBackupHttpErrorURLPrefix;
    config.backup_network_error_url_prefix =
        ::safe_browsing::kSbBackupNetworkErrorURLPrefix;
    ping_manager_ = ::safe_browsing::BasePingManager::Create(
        network_context_->GetURLLoaderFactory(), config);
  }

  if (!serialized.empty()) {
    DVLOG(1) << "Sending serialized threat details";
    ping_manager_->ReportThreatDetails(serialized);
  }
}

void AwSafeBrowsingUIManager::CreateURLLoaderFactoryForIO(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network_context_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(request), 0);
}

}  // namespace android_webview
