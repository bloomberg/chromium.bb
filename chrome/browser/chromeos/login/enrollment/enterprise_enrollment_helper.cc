// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_impl.h"

namespace chromeos {

EnterpriseEnrollmentHelper::~EnterpriseEnrollmentHelper() {
}

// static
scoped_ptr<EnterpriseEnrollmentHelper> EnterpriseEnrollmentHelper::Create(
    EnrollmentStatusConsumer* status_consumer,
    EnrollmentMode enrollment_mode,
    std::string& domain) {
  return make_scoped_ptr(new EnterpriseEnrollmentHelperImpl(
      status_consumer, enrollment_mode, domain));
}

EnterpriseEnrollmentHelper::EnterpriseEnrollmentHelper(
    EnrollmentStatusConsumer* status_consumer)
    : status_consumer_(status_consumer) {
  DCHECK(status_consumer_);
}

}  // namespace chromeos
