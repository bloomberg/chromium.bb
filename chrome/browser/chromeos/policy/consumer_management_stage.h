// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_STAGE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_STAGE_H_

namespace policy {

// The consumer management stage is a value indicating the current stage of the
// enrollment or the unenrollment process. This class has a bunch of methods
// that return a boolean based on the stage. The callers should use those
// methods instead of the internal value.
class ConsumerManagementStage {
 public:
  // The following static methods return objects for each stage. For each stage,
  // one or more boolean member methods may return true. The methods which
  // return true are documented in the parentheses for each stage.
  //
  // Nothing is going on.
  static ConsumerManagementStage None();
  // Enrollment was requested by the owner.
  // (IsEnrolling, IsEnrollmentRequested)
  static ConsumerManagementStage EnrollmentRequested();
  // The owner ID was stored in the boot lockbox.
  // (IsEnrolling)
  static ConsumerManagementStage EnrollmentOwnerStored();
  // The enrollment process has succeeded.
  // (HasPendingNotification, HasEnrollmentSucceeded)
  static ConsumerManagementStage EnrollmentSuccess();
  // The enrollment process was canceled by the user.
  // (HasPendingNotification, HasEnrollmentFailed)
  static ConsumerManagementStage EnrollmentCanceled();
  // Failed to write to the boot lockbox.
  // (HasPendingNotification, HasEnrollmentFailed)
  static ConsumerManagementStage EnrollmentBootLockboxFailed();
  // Failed to get the access token.
  // (HasPendingNotification, HasEnrollmentFailed)
  static ConsumerManagementStage EnrollmentGetTokenFailed();
  // Failed to register the device.
  // (HasPendingNotification, HasEnrollmentFailed)
  static ConsumerManagementStage EnrollmentDMServerFailed();
  // Unenrollment was requested by the owner.
  // (IsUnenrolling)
  static ConsumerManagementStage UnenrollmentRequested();
  // The unenrollment process has succeeded.
  // (HasPengindNotification, HasUnenrollmentSucceeded)
  static ConsumerManagementStage UnenrollmentSuccess();
  // Failed to unregister the device.
  // (HasPendingNotification, HasUnenrollmentFailed)
  static ConsumerManagementStage UnenrollmentDMServerFailed();
  // Failed to update the device settings.
  // (HasPendingNotification, HasUnenrollmentFailed)
  static ConsumerManagementStage UnenrollmentUpdateDeviceSettingsFailed();

  // Copyable.
  ConsumerManagementStage(const ConsumerManagementStage& other) = default;
  ConsumerManagementStage& operator=(
      const ConsumerManagementStage& other) = default;

  bool operator==(const ConsumerManagementStage& other) const;

  // Returns true if there's a notification that should be shown.
  bool HasPendingNotification() const;

  // Returns true if the enrollment process is in progress.
  bool IsEnrolling() const;

  // Returns true if enrollment is requested.
  bool IsEnrollmentRequested() const;

  // Returns true if the enrollment process succeeded.
  bool HasEnrollmentSucceeded() const;

  // Returns true if the enrollment process failed.
  bool HasEnrollmentFailed() const;

  // Returns true if the unenrollment process is in progress.
  bool IsUnenrolling() const;

  // Returns true if the unenrollment process succeeded.
  bool HasUnenrollmentSucceeded() const;

  // Returns true if the unenrollment process failed.
  bool HasUnenrollmentFailed() const;

  // Returns an instance from an internal value. Returns |None()| if |int_value|
  // is out of range. This should only be used in de-serialization.
  static ConsumerManagementStage FromInternalValue(int int_value);

  // Returns the internal value. This should only be used in serialization.
  int ToInternalValue() const;

 private:
  // Note that the numerical values are stored in local state. They should never
  // be changed.
  enum Value {
    NONE = 0,
    ENROLLMENT_REQUESTED = 1,
    ENROLLMENT_OWNER_STORED = 2,
    ENROLLMENT_SUCCESS = 3,
    ENROLLMENT_CANCELED = 4,
    ENROLLMENT_BOOT_LOCKBOX_FAILED = 5,
    ENROLLMENT_GET_TOKEN_FAILED = 6,
    ENROLLMENT_DM_SERVER_FAILED = 7,
    UNENROLLMENT_REQUESTED = 8,
    UNENROLLMENT_SUCCESS = 9,
    UNENROLLMENT_DM_SERVER_FAILED = 10,
    UNENROLLMENT_UPDATE_DEVICE_SETTINGS_FAILED = 11,
    LAST,  // This should always be the last one.
  };

  explicit ConsumerManagementStage(Value value);

  Value value_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CONSUMER_MANAGEMENT_STAGE_H_
