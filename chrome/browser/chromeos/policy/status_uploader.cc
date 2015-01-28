// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace {
const int kMinUploadDelayMs = 60 * 1000;  // 60 seconds
}  // namespace

namespace policy {

const int64 StatusUploader::kDefaultUploadDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours

StatusUploader::StatusUploader(
    PrefService* local_state,
    CloudPolicyClient* client,
    scoped_ptr<DeviceStatusCollector> collector,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      collector_(collector.Pass()),
      task_runner_(task_runner),
      weak_factory_(this) {
  // StatusUploader is currently only created for registered clients, and
  // it is currently safe to assume that the client will not unregister while
  // StatusUploader is alive.
  //
  // If future changes result in StatusUploader's lifetime extending beyond
  // unregistration events, then this class should be updated
  // to skip status uploads for unregistered clients, and to observe the client
  // and kick off an upload when registration happens.
  DCHECK(client->is_registered());

  // Listen for changes to the upload delay, and start sending updates to the
  // server.
  upload_delay_.reset(new IntegerPrefMember());
  upload_delay_->Init(
      prefs::kDeviceStatusUploadRate, local_state,
      base::Bind(&StatusUploader::ScheduleNextStatusUpload,
                 base::Unretained(this)));
  ScheduleNextStatusUpload();
}

StatusUploader::~StatusUploader() {
}

void StatusUploader::ScheduleNextStatusUpload() {
  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of 0).
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(
      std::max(kMinUploadDelayMs, upload_delay_->GetValue()));
  base::TimeDelta delay =
      std::max((last_upload_ + delta) - base::Time::NowFromSystemTime(),
               base::TimeDelta());
  upload_callback_.Reset(base::Bind(&StatusUploader::UploadStatus,
                                    base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, upload_callback_.callback(), delay);
}

void StatusUploader::UploadStatus() {
  // If we already have an upload request active, don't bother starting another
  // one.
  if (request_job_)
    return;

  enterprise_management::DeviceStatusReportRequest device_status;
  bool have_device_status = collector_->GetDeviceStatus(&device_status);
  enterprise_management::SessionStatusReportRequest session_status;
  bool have_session_status = collector_->GetDeviceSessionStatus(
      &session_status);
  if (!have_device_status && !have_session_status) {
    // Don't have any status to upload - just set our timer for next time.
    last_upload_ = base::Time::NowFromSystemTime();
    ScheduleNextStatusUpload();
    return;
  }

  client_->UploadDeviceStatus(
      have_device_status ? &device_status : nullptr,
      have_session_status ? &session_status : nullptr,
      base::Bind(&StatusUploader::OnUploadCompleted,
                 weak_factory_.GetWeakPtr()));
}

void StatusUploader::OnUploadCompleted(bool success) {
  // Set the last upload time, regardless of whether the upload was successful
  // or not (we don't change the time of the next upload based on whether this
  // upload succeeded or not - if a status upload fails, we just skip it and
  // wait until it's time to try again.
  last_upload_ = base::Time::NowFromSystemTime();

  // If the upload was successful, tell the collector so it can clear its cache
  // of pending items.
  if (success)
    collector_->OnSubmittedSuccessfully();

  ScheduleNextStatusUpload();
}

}  // namespace policy
