// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"

namespace chromeos {
namespace test {

namespace ui {

const char kEnrollmentStepSuccess[] = "success";
const char kEnrollmentStepError[] = "error";
const char kEnrollmentStepLicenses[] = "license";
const char kEnrollmentStepDeviceAttributes[] = "attribute-prompt";
const char kEnrollmentStepADJoin[] = "ad-join";
const char kEnrollmentStepADJoinError[] = "active-directory-join-error";

}  // namespace ui

namespace values {

const char kLicenseTypePerpetual[] = "perpetual";
const char kLicenseTypeAnnual[] = "annual";
const char kLicenseTypeKiosk[] = "kiosk";

const char kAssetId[] = "asset_id";
const char kLocation[] = "location";

}  // namespace values

namespace {

const char* const kAllSteps[] = {
    ui::kEnrollmentStepLicenses, ui::kEnrollmentStepDeviceAttributes,
    ui::kEnrollmentStepSuccess,  ui::kEnrollmentStepADJoin,
    ui::kEnrollmentStepError,    ui::kEnrollmentStepADJoinError};

std::string StepVisibleExpression(const std::string& step) {
  return "document.getElementsByClassName('oauth-enroll-state-" + step +
         "').length > 0";
}

}  // namespace

EnrollmentUIMixin::EnrollmentUIMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

EnrollmentUIMixin::~EnrollmentUIMixin() = default;

// Waits until specific enrollment step is displayed.
void EnrollmentUIMixin::WaitForStep(const std::string& step) {
  OobeJS().CreateWaiter(StepVisibleExpression(step))->Wait();
  for (const char* other : kAllSteps) {
    if (other != step) {
      ASSERT_FALSE(IsStepDisplayed(other));
    }
  }
}
// Returns true if there are any DOM elements with the given class.
bool EnrollmentUIMixin::IsStepDisplayed(const std::string& step) {
  return OobeJS().GetBool(StepVisibleExpression(step));
}

void EnrollmentUIMixin::SelectEnrollmentLicense(
    const std::string& license_type) {
  OobeJS().SelectRadioPath(
      {"oauth-enroll-license-ui", "license-option-" + license_type});
}

void EnrollmentUIMixin::UseSelectedLicense() {
  OobeJS().TapOnPath({"oauth-enroll-license-ui", "next"});
}

void EnrollmentUIMixin::SubmitDeviceAttributes(const std::string& asset_id,
                                               const std::string& location) {
  OobeJS().TypeIntoPath(asset_id, {"oauth-enroll-asset-id"});
  OobeJS().TypeIntoPath(location, {"oauth-enroll-location"});
  OobeJS().TapOn("enroll-attributes-submit-button");
}

}  // namespace test
}  // namespace chromeos
