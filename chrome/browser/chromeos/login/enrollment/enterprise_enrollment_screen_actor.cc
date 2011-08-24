// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"

namespace chromeos {

EnterpriseEnrollmentScreenActor::EnterpriseEnrollmentScreenActor() {}

EnterpriseEnrollmentScreenActor::~EnterpriseEnrollmentScreenActor() {}

void EnterpriseEnrollmentScreenActor::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void EnterpriseEnrollmentScreenActor::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void EnterpriseEnrollmentScreenActor::NotifyObservers(bool succeeded) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnEnrollmentComplete(this, succeeded));
}

}  // namespace chromeos
