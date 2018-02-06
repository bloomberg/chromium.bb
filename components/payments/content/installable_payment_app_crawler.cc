// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/installable_payment_app_crawler.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace payments {

// TODO(crbug.com/782270): Use cache to accelerate crawling procedure.
// TODO(crbug.com/782270): Add integration tests for this class.
InstallablePaymentAppCrawler::InstallablePaymentAppCrawler(
    PaymentManifestDownloader* downloader,
    PaymentManifestParser* parser,
    PaymentManifestWebDataService* cache)
    : downloader_(downloader),
      parser_(parser),
      number_of_payment_method_manifest_to_download_(0),
      number_of_payment_method_manifest_to_parse_(0),
      number_of_web_app_manifest_to_download_(0),
      number_of_web_app_manifest_to_parse_(0),
      weak_ptr_factory_(this) {}

InstallablePaymentAppCrawler::~InstallablePaymentAppCrawler() {}

void InstallablePaymentAppCrawler::Start(
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    FinishedCrawlingCallback callback,
    base::OnceClosure finished_using_resources) {
  callback_ = std::move(callback);
  finished_using_resources_ = std::move(finished_using_resources);

  std::set<GURL> manifests_to_download;
  for (const auto& method_data : requested_method_data) {
    for (const auto& method_name : method_data->supported_methods) {
      if (!base::IsStringUTF8(method_name))
        continue;
      GURL url = GURL(method_name);
      if (url.is_valid()) {
        manifests_to_download.insert(url);
      }
    }
  }

  if (manifests_to_download.empty()) {
    // Post the result back asynchronously.
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(
            &InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  number_of_payment_method_manifest_to_download_ = manifests_to_download.size();
  for (const auto& url : manifests_to_download) {
    downloader_->DownloadPaymentMethodManifest(
        url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentMethodManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestDownloaded(
    const GURL& method_manifest_url,
    const std::string& content) {
  number_of_payment_method_manifest_to_download_--;
  if (content.empty()) {
    FinishCrawlingPaymentAppsIfReady();
    return;
  }

  number_of_payment_method_manifest_to_parse_++;
  parser_->ParsePaymentMethodManifest(
      content, base::BindOnce(
                   &InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed,
                   weak_ptr_factory_.GetWeakPtr(), method_manifest_url));
}

void InstallablePaymentAppCrawler::OnPaymentMethodManifestParsed(
    const GURL& method_manifest_url,
    const std::vector<GURL>& default_applications,
    const std::vector<url::Origin>& supported_origins,
    bool all_origins_supported) {
  number_of_payment_method_manifest_to_parse_--;

  for (const auto& url : default_applications) {
    if (downloaded_web_app_manifests_.find(url) !=
        downloaded_web_app_manifests_.end()) {
      // Do not download the same web app manifest again since a web app could
      // be the default application of multiple payment methods.
      continue;
    }

    number_of_web_app_manifest_to_download_++;
    downloaded_web_app_manifests_.insert(url);
    downloader_->DownloadWebAppManifest(
        url,
        base::BindOnce(
            &InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded,
            weak_ptr_factory_.GetWeakPtr(), method_manifest_url, url));
  }

  FinishCrawlingPaymentAppsIfReady();
}

void InstallablePaymentAppCrawler::OnPaymentWebAppManifestDownloaded(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    const std::string& content) {
  number_of_web_app_manifest_to_download_--;
  if (content.empty()) {
    FinishCrawlingPaymentAppsIfReady();
    return;
  }

  number_of_web_app_manifest_to_parse_++;
  parser_->ParseWebAppInstallationInfo(
      content,
      base::BindOnce(
          &InstallablePaymentAppCrawler::OnPaymentWebAppInstallationInfo,
          weak_ptr_factory_.GetWeakPtr(), method_manifest_url,
          web_app_manifest_url));
}

void InstallablePaymentAppCrawler::OnPaymentWebAppInstallationInfo(
    const GURL& method_manifest_url,
    const GURL& web_app_manifest_url,
    std::unique_ptr<WebAppInstallationInfo> app_info) {
  number_of_web_app_manifest_to_parse_--;

  if (app_info != nullptr) {
    if (app_info->sw_js_url.empty() ||
        !base::IsStringUTF8(app_info->sw_js_url)) {
      FinishCrawlingPaymentAppsIfReady();
      LOG(ERROR) << "The installable payment app's js url is not a non-empty "
                    "UTF8 string.";
      return;
    }

    // Check and complete relative url.
    if (!GURL(app_info->sw_js_url).is_valid()) {
      GURL absolute_url = web_app_manifest_url.Resolve(app_info->sw_js_url);
      if (!absolute_url.is_valid()) {
        LOG(ERROR) << "Failed to resolve the installable payment app's js url.";
        FinishCrawlingPaymentAppsIfReady();
        return;
      }
      app_info->sw_js_url = absolute_url.spec();
    }

    if (!GURL(app_info->sw_scope).is_valid()) {
      GURL absolute_scope =
          web_app_manifest_url.GetWithoutFilename().Resolve(app_info->sw_scope);
      if (!absolute_scope.is_valid()) {
        LOG(ERROR) << "Failed to resolve the installable payment app's "
                      "registration scope.";
        FinishCrawlingPaymentAppsIfReady();
        return;
      }
      app_info->sw_scope = absolute_scope.spec();
    }

    installable_apps_[method_manifest_url] = std::move(app_info);
  }

  FinishCrawlingPaymentAppsIfReady();
}

void InstallablePaymentAppCrawler::FinishCrawlingPaymentAppsIfReady() {
  if (number_of_payment_method_manifest_to_download_ != 0 ||
      number_of_payment_method_manifest_to_parse_ != 0 ||
      number_of_web_app_manifest_to_download_ != 0 ||
      number_of_web_app_manifest_to_parse_ != 0) {
    return;
  }

  std::move(callback_).Run(std::move(installable_apps_));
  std::move(finished_using_resources_).Run();
}

}  // namespace payments.
