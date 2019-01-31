// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"

#include <string>

#include "base/bind.h"
#include "base/guid.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/background_service/download_service.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace plugin_vm {

bool PluginVmImageManager::IsDownloading() {
  return processing_download_;
}

void PluginVmImageManager::StartDownload() {
  if (IsDownloading())
    return OnDownloadFailed();
  processing_download_ = true;
  download_service_->StartDownload(GetDownloadParams());
}

void PluginVmImageManager::CancelDownload() {
  DCHECK(!current_download_guid_.empty());
  download_service_->CancelDownload(current_download_guid_);
}

void PluginVmImageManager::SetObserver(Observer* observer) {
  observer_ = observer;
}

void PluginVmImageManager::OnDownloadStarted() {
  if (observer_)
    observer_->OnDownloadStarted();
}

void PluginVmImageManager::OnProgressUpdated(
    base::Optional<double> fraction_complete) {
  if (observer_)
    observer_->OnProgressUpdated(fraction_complete);
}

void PluginVmImageManager::OnDownloadCompleted(base::FilePath file_path) {
  if (observer_)
    observer_->OnDownloadCompleted(file_path);
  current_download_guid_.clear();
  processing_download_ = false;
}

void PluginVmImageManager::OnDownloadCancelled() {
  if (observer_)
    observer_->OnDownloadCancelled();
  current_download_guid_.clear();
  processing_download_ = false;
}

void PluginVmImageManager::OnDownloadFailed() {
  if (observer_)
    observer_->OnDownloadFailed();
  current_download_guid_.clear();
  processing_download_ = false;
}

void PluginVmImageManager::SetDownloadServiceForTesting(
    download::DownloadService* download_service) {
  download_service_ = download_service;
}

std::string PluginVmImageManager::GetCurrentDownloadGuidForTesting() {
  return current_download_guid_;
}

PluginVmImageManager::PluginVmImageManager(Profile* profile)
    : profile_(profile),
      download_service_(DownloadServiceFactory::GetForBrowserContext(profile)) {
}
PluginVmImageManager::~PluginVmImageManager() = default;

download::DownloadParams PluginVmImageManager::GetDownloadParams() {
  download::DownloadParams params;

  // DownloadParams
  params.client = download::DownloadClient::PLUGIN_VM_IMAGE;
  params.guid = base::GenerateGUID();
  params.callback = base::BindRepeating(&PluginVmImageManager::OnStarted,
                                        base::Unretained(this));
  // TODO(okalitova): Create annotation.
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(NO_TRAFFIC_ANNOTATION_YET);

  // RequestParams
  std::string url;
  profile_->GetPrefs()
      ->GetDictionary(plugin_vm::prefs::kPluginVmImage)
      ->GetString("url", &url);
  params.request_params.url = GURL(url);
  params.request_params.method = "GET";

  // SchedulingParams
  // User initiates download by clicking on PluginVm icon so priorities should
  // be the highest.
  params.scheduling_params.priority = download::SchedulingParams::Priority::UI;
  params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;

  return params;
}

void PluginVmImageManager::OnStarted(
    const std::string& download_guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::ACCEPTED)
    current_download_guid_ = download_guid;
  else
    OnDownloadFailed();
}

}  // namespace plugin_vm
