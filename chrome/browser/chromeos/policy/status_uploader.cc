// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace {
const int kMinUploadDelayMs = 60 * 1000;  // 60 seconds
}  // namespace

namespace policy {

const int64 StatusUploader::kDefaultUploadDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours

StatusUploader::StatusUploader(
    CloudPolicyClient* client,
    scoped_ptr<DeviceStatusCollector> collector,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      collector_(collector.Pass()),
      task_runner_(task_runner),
      upload_frequency_(base::TimeDelta::FromMilliseconds(
          kDefaultUploadDelayMs)),
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
  upload_frequency_observer_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kReportUploadFrequency,
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     base::Unretained(this)));

  // Update the upload frequency from settings.
  RefreshUploadFrequency();

  // Immediately schedule our next status upload (last_upload_ is set to the
  // start of the epoch, so this will trigger an update in the immediate
  // future).
  ScheduleNextStatusUpload();
}

StatusUploader::~StatusUploader() {
}

void StatusUploader::ScheduleNextStatusUpload() {
  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of 0).
  base::TimeDelta delay = std::max(
      (last_upload_ + upload_frequency_) - base::Time::NowFromSystemTime(),
      base::TimeDelta());
  upload_callback_.Reset(base::Bind(&StatusUploader::UploadStatus,
                                    base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, upload_callback_.callback(), delay);
}

void StatusUploader::RefreshUploadFrequency() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED != settings->PrepareTrustedValues(
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - update our cached upload_frequency (we cache the
  // value because CrosSettings can become untrusted at arbitrary times and we
  // want to use the last trusted value).
  int frequency;
  if (settings->GetInteger(chromeos::kReportUploadFrequency, &frequency)) {
      upload_frequency_ = base::TimeDelta::FromMilliseconds(
          std::max(kMinUploadDelayMs, frequency));
  }

  // Schedule a new upload with the new frequency - only do this if we've
  // already performed the initial upload, because we want the initial upload
  // to happen immediately on startup and not get cancelled by settings changes.
  if (!last_upload_.is_null())
    ScheduleNextStatusUpload();
}

void StatusUploader::UploadStatus() {
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
