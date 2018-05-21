// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/browser_download_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#include "ios/chrome/browser/download/pass_kit_mime_type.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#include "ios/web/public/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns DownloadMimeTypeResult for the given MIME type.
DownloadMimeTypeResult GetUmaResult(const std::string& mime_type) {
  if (mime_type == kPkPassMimeType)
    return DownloadMimeTypeResult::PkPass;

  if (mime_type == "application/x-apple-aspen-config")
    return DownloadMimeTypeResult::iOSMobileConfig;

  return DownloadMimeTypeResult::Other;
}
}  // namespace

BrowserDownloadService::BrowserDownloadService(
    web::DownloadController* download_controller)
    : download_controller_(download_controller) {
  DCHECK(!download_controller->GetDelegate());
  download_controller_->SetDelegate(this);
}

BrowserDownloadService::~BrowserDownloadService() {
  if (download_controller_) {
    DCHECK_EQ(this, download_controller_->GetDelegate());
    download_controller_->SetDelegate(nullptr);
  }
}

void BrowserDownloadService::OnDownloadCreated(
    web::DownloadController* download_controller,
    web::WebState* web_state,
    std::unique_ptr<web::DownloadTask> task) {
  UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadMimeType",
                            GetUmaResult(task->GetMimeType()));

  if (task->GetMimeType() == kPkPassMimeType) {
    PassKitTabHelper* tab_helper = PassKitTabHelper::FromWebState(web_state);
    if (tab_helper) {
      tab_helper->Download(std::move(task));
    }
  } else {
    if (base::FeatureList::IsEnabled(web::features::kNewFileDownload)) {
      DownloadManagerTabHelper* tab_helper =
          DownloadManagerTabHelper::FromWebState(web_state);
      if (tab_helper) {
        tab_helper->Download(std::move(task));
      }
    }
  }
}

void BrowserDownloadService::OnDownloadControllerDestroyed(
    web::DownloadController* download_controller) {
  DCHECK_EQ(this, download_controller->GetDelegate());
  download_controller->SetDelegate(nullptr);
  download_controller_ = nullptr;
}
