// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_STATUS_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_STATUS_UPLOADER_H_

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "policy/proto/device_management_backend.pb.h"

namespace base {
class PrefService;
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyClient;
class DeviceManagementRequestJob;
class DeviceStatusCollector;

// Class responsible for periodically uploading device status from the
// passed DeviceStatusCollector.
class StatusUploader {
 public:
  // Refresh constants.
  static const int64 kDefaultUploadDelayMs;

  // Constructor. |client| must be registered and must stay
  // valid and registered through the lifetime of this StatusUploader
  // object.
  StatusUploader(
      PrefService* local_state,
      CloudPolicyClient* client,
      scoped_ptr<DeviceStatusCollector> collector,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ~StatusUploader();

  // Returns the time of the last successful upload, or Time(0) if no upload
  // has ever happened.
  base::Time last_upload() const { return last_upload_; }

 private:
  // Callback invoked periodically to upload the device status from the
  // DeviceStatusCollector.
  void UploadStatus();

  // Invoked once a status upload has completed.
  void OnUploadCompleted(bool success);

  // Helper method that figures out when the next status upload should
  // be scheduled.
  void ScheduleNextStatusUpload();

  // CloudPolicyClient used to issue requests to the server.
  CloudPolicyClient* client_;

  // The job associated with any ongoing requests to the cloud. We currently
  // only support a single active request at a time.
  scoped_ptr<DeviceManagementRequestJob> request_job_;

  // DeviceStatusCollector that provides status for uploading.
  scoped_ptr<DeviceStatusCollector> collector_;

  // TaskRunner used for scheduling upload tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Pref item that specifies what our upload delay is currently.
  scoped_ptr<IntegerPrefMember> upload_delay_;

  // The time the last upload was performed.
  base::Time last_upload_;

  // Callback invoked via a delay to upload device status.
  base::CancelableClosure upload_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<StatusUploader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StatusUploader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_STATUS_UPLOADER_H_
