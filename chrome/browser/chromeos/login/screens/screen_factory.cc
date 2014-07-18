// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/screen_factory.h"

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_creation_screen.h"

namespace chromeos {

// static
const char ScreenFactory::kEnrollmentScreenId[] = "enroll";
const char ScreenFactory::kErrorScreenId[] = "error-message";
const char ScreenFactory::kEulaScreenId[] = "eula";
const char ScreenFactory::kKioskAutolaunchScreenId[] = "autolaunch";
const char ScreenFactory::kNetworkScreenId[] = "network";
const char ScreenFactory::kResetScreenId[] = "reset";
const char ScreenFactory::kSupervisedUserCreationScreenId[] =
  "locally-managed-user-creation-flow";
const char ScreenFactory::kTermsOfServiceScreenId[] = "tos";
const char ScreenFactory::kUpdateScreenId[] = "update";
const char ScreenFactory::kUserImageScreenId[] = "image";
const char ScreenFactory::kWrongHWIDScreenId[] = "wrong-hwid";

const char ScreenFactory::kLoginScreenId[] = "login";

ScreenFactory::ScreenFactory(ScreenObserver* observer,
                             OobeDisplay* oobe_display)
    : observer_(observer),
      oobe_display_(oobe_display) {
}

ScreenFactory::~ScreenFactory() {
}

BaseScreen* ScreenFactory::CreateScreen(const std::string& id) {
  BaseScreen* result = CreateScreenImpl(id);
  DCHECK_EQ(id, result->GetID());
  return result;
}

BaseScreen* ScreenFactory::CreateScreenImpl(const std::string& id) {
  if (id == kNetworkScreenId) {
    return new NetworkScreen(observer_, oobe_display_->GetNetworkScreenActor());
  } else if (id == kUpdateScreenId) {
    return new UpdateScreen(observer_, oobe_display_->GetUpdateScreenActor());
  } else if (id == kUserImageScreenId) {
    return new UserImageScreen(observer_,
                               oobe_display_->GetUserImageScreenActor());
  } else if (id == kEulaScreenId) {
    return new EulaScreen(observer_, oobe_display_->GetEulaScreenActor());
  } else if (id == kEnrollmentScreenId) {
    return new EnrollmentScreen(observer_,
                                oobe_display_->GetEnrollmentScreenActor());
  } else if (id == kResetScreenId) {
    return new ResetScreen(observer_, oobe_display_->GetResetScreenActor());
  } else if (id == kKioskAutolaunchScreenId) {
    return new KioskAutolaunchScreen(
        observer_, oobe_display_->GetKioskAutolaunchScreenActor());
  } else if (id == kTermsOfServiceScreenId) {
    return new TermsOfServiceScreen(observer_,
        oobe_display_->GetTermsOfServiceScreenActor());
  } else if (id == kWrongHWIDScreenId) {
    return new WrongHWIDScreen(
        observer_,
        oobe_display_->GetWrongHWIDScreenActor());
  } else if (id == kSupervisedUserCreationScreenId) {
    return new SupervisedUserCreationScreen(
        observer_,
        oobe_display_->GetSupervisedUserCreationScreenActor());
  } else if (id == kErrorScreenId) {
    return new ErrorScreen(observer_, oobe_display_->GetErrorScreenActor());
  }

  // TODO(antrim): support for login screen.
  NOTREACHED() << "Unknown screen ID: " << id;
  return NULL;
}

}  // namespace chromeos
