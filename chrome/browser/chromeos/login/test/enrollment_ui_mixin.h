// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ENROLLMENT_UI_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ENROLLMENT_UI_MIXIN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"

namespace chromeos {
namespace test {

namespace ui {

//  WaitForStep(...) constants.

extern const char kEnrollmentStepSignin[];
extern const char kEnrollmentStepLicenses[];
extern const char kEnrollmentStepDeviceAttributes[];
extern const char kEnrollmentStepSuccess[];
extern const char kEnrollmentStepADJoin[];
extern const char kEnrollmentStepError[];
extern const char kEnrollmentStepDeviceAttributesError[];
extern const char kEnrollmentStepADJoinError[];

}  // namespace ui

namespace values {

// SelectEnrollmentLicense(...) constants.

extern const char kLicenseTypePerpetual[];
extern const char kLicenseTypeKiosk[];
extern const char kLicenseTypeAnnual[];

// SubmitDeviceAttributes common values.

extern const char kAssetId[];
extern const char kLocation[];

}  // namespace values

// This test mixin covers enrollment-specific OOBE UI interactions.
class EnrollmentUIMixin : public InProcessBrowserTestMixin {
 public:
  explicit EnrollmentUIMixin(InProcessBrowserTestMixinHost* host);
  ~EnrollmentUIMixin() override;

  // Waits until specific enrollment step is displayed.
  void WaitForStep(const std::string& step);
  bool IsStepDisplayed(const std::string& step);

  void ExpectErrorMessage(int error_message_id, bool can_retry);
  void RetryAfterError();

  // Fills out the UI with device attribute information and submits it.
  void SubmitDeviceAttributes(const std::string& asset_id,
                              const std::string& location);

  void LeaveDeviceAttributeErrorScreen();

  // Selects enrollment license.
  void SelectEnrollmentLicense(const std::string& license_type);

  // Proceeds with selected license.
  void UseSelectedLicense();

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentUIMixin);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_ENROLLMENT_UI_MIXIN_H_
