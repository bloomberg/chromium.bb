// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

class GoogleServiceAuthError;
class Profile;

namespace policy {
struct EnrollmentConfig;
class EnrollmentStatus;
}

namespace chromeos {

// This class is capable to enroll the device into enterprise domain, using
// either a profile containing authentication data or OAuth token.
// It can also clear an authentication data from the profile and revoke tokens
// that are not longer needed.
class EnterpriseEnrollmentHelper {
 public:
  typedef policy::DeviceCloudPolicyInitializer::EnrollmentCallback
      EnrollmentCallback;

  // Enumeration of the possible errors that can occur during enrollment which
  // are not covered by GoogleServiceAuthError or EnrollmentStatus.
  enum OtherError {
    // Existing enrollment domain doesn't match authentication user.
    OTHER_ERROR_DOMAIN_MISMATCH,
    // Unexpected error condition, indicates a bug in the code.
    OTHER_ERROR_FATAL
  };

  class EnrollmentStatusConsumer {
   public:
    // Called when an error happens on attempt to receive authentication tokens.
    virtual void OnAuthError(const GoogleServiceAuthError& error) = 0;

    // Called when an error happens during enrollment.
    virtual void OnEnrollmentError(policy::EnrollmentStatus status) = 0;

    // Called when some other error happens.
    virtual void OnOtherError(OtherError error) = 0;

    // Called when enrollment finishes successfully. |additional_token| keeps
    // the additional access token, if it was requested by setting the
    // |fetch_additional_token| param of EnrollUsingProfile() to true.
    // Otherwise, |additional_token| is empty.
    virtual void OnDeviceEnrolled(const std::string& additional_token) = 0;
  };

  // Factory method. Caller takes ownership of the returned object.
  static scoped_ptr<EnterpriseEnrollmentHelper> Create(
      EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain);

  virtual ~EnterpriseEnrollmentHelper();

  // Starts enterprise enrollment using |token|.
  // EnrollUsingToken can be called only once during this object's lifetime, and
  // only if EnrollUsingProfile was not called before.
  virtual void EnrollUsingToken(const std::string& token) = 0;

  // Starts enterprise enrollment using |profile|. First tries to fetch an
  // authentication token using the |profile|, then tries to enroll the device
  // with the received token.
  // If |fetch_additional_token| is true, the helper fetches an additional token
  // and passes it to the |status_consumer| on successfull enrollment.
  // If enrollment fails, you should clear authentication data in |profile| by
  // calling ClearAuth before destroying |this|.
  // EnrollUsingProfile can be called only once during this object's lifetime,
  // and only if EnrollUsingToken was not called before.
  virtual void EnrollUsingProfile(Profile* profile,
                                  bool fetch_additional_token) = 0;

  // Clears authentication data from the profile (if EnrollUsingProfile was
  // used) and revokes fetched tokens.
  // Does not revoke the additional token if enrollment finished successfully.
  // Calls |callback| on completion.
  virtual void ClearAuth(const base::Closure& callback) = 0;

 protected:
  // |status_consumer| must outlive |this|. Moreover, the user of this class
  // is responsible for clearing auth data in some cases (see comment for
  // EnrollUsingProfile()).
  explicit EnterpriseEnrollmentHelper(
      EnrollmentStatusConsumer* status_consumer);

  EnrollmentStatusConsumer* status_consumer() { return status_consumer_; }

 private:
  EnrollmentStatusConsumer* status_consumer_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_
