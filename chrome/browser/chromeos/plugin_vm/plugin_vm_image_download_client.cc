// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace plugin_vm {

PluginVmImageDownloadClient::PluginVmImageDownloadClient(Profile* profile)
    : profile_(profile) {}
PluginVmImageDownloadClient::~PluginVmImageDownloadClient() = default;

PluginVmImageManager* PluginVmImageDownloadClient::GetManager() {
  return PluginVmImageManagerFactory::GetForProfile(profile_);
}

// TODO(okalitova): Remove logs.

void PluginVmImageDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  VLOG(1) << __func__ << " called";
  // TODO(okalitova): Manage downloads after sleep and log out.
  for (const auto& download : downloads) {
    VLOG(1) << "Download tracked by DownloadService: " << download.guid;
    DownloadServiceFactory::GetForBrowserContext(profile_)->CancelDownload(
        download.guid);
  }
}

void PluginVmImageDownloadClient::OnServiceUnavailable() {
  VLOG(1) << __func__ << " called";
}

download::Client::ShouldDownload PluginVmImageDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  VLOG(1) << __func__ << " called";
  content_length_ = headers->GetContentLength();
  GetManager()->OnDownloadStarted();
  return download::Client::ShouldDownload::CONTINUE;
}

void PluginVmImageDownloadClient::OnDownloadUpdated(const std::string& guid,
                                                    uint64_t bytes_uploaded,
                                                    uint64_t bytes_downloaded) {
  VLOG(1) << __func__ << " called";
  VLOG(1) << bytes_downloaded << " bytes downloaded";
  GetManager()->OnProgressUpdated(GetFractionComplete(bytes_downloaded));
}

void PluginVmImageDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& completion_info,
    download::Client::FailureReason reason) {
  VLOG(1) << __func__ << " called";
  switch (reason) {
    case download::Client::FailureReason::NETWORK:
      VLOG(1) << "Failure reason: NETWORK";
      break;
    case download::Client::FailureReason::UPLOAD_TIMEDOUT:
      VLOG(1) << "Failure reason: UPLOAD_TIMEDOUT";
      break;
    case download::Client::FailureReason::TIMEDOUT:
      VLOG(1) << "Failure reason: TIMEDOUT";
      break;
    case download::Client::FailureReason::UNKNOWN:
      VLOG(1) << "Failure reason: UNKNOWN";
      break;
    case download::Client::FailureReason::ABORTED:
      VLOG(1) << "Failure reason: ABORTED";
      break;
    case download::Client::FailureReason::CANCELLED:
      VLOG(1) << "Failure reason: CANCELLED";
      break;
  }

  if (reason == download::Client::FailureReason::CANCELLED)
    GetManager()->OnDownloadCancelled();
  else
    GetManager()->OnDownloadFailed();
}

void PluginVmImageDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  VLOG(1) << __func__ << " called";
  VLOG(1) << "Downloaded file is in " << completion_info.path.value();
  // TODO(https://crbug.com/904851): Verify download using hash specified by
  // PluginVmImage user policy. If hashes don't match remove downloaded file.
  GetManager()->OnDownloadCompleted(completion_info.path);
}

bool PluginVmImageDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  VLOG(1) << __func__ << " called";
  return true;
}

void PluginVmImageDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  VLOG(1) << __func__ << " called";
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

base::Optional<double> PluginVmImageDownloadClient::GetFractionComplete(
    int64_t bytes_downloaded) {
  if (content_length_ == -1 || content_length_ == 0)
    return base::nullopt;
  if (bytes_downloaded > content_length_)
    return base::nullopt;
  return base::make_optional(static_cast<double>(bytes_downloaded) /
                             content_length_);
}

}  // namespace plugin_vm
