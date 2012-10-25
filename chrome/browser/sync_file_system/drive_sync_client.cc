// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_sync_client.h"

#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"

namespace sync_file_system {

DriveSyncClient::DriveSyncClient(Profile* profile) {
  drive_service_.reset(new google_apis::GDataWapiService);
  drive_service_->AddObserver(this);
  drive_service_->Initialize(profile);

  drive_uploader_.reset(new google_apis::DriveUploader(drive_service_.get()));
}

DriveSyncClient::~DriveSyncClient() {
  DCHECK(CalledOnValidThread());
}

void DriveSyncClient::OnReadyToPerformOperations() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::OnAuthenticationFailed(
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::GetDriveFolderForSyncData(
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::GetDriveFolderForOrigin(
    const GURL& origin,
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::GetLargestChangeStamp(
    const ChangeStampCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::ListFiles(std::string resource_id,
                                const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::ListChanges(int64 start_changestamp,
                                  const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

void DriveSyncClient::ContinueListing(const GURL& feed_url,
                                      const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace sync_file_system
