// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"

namespace policy {

class FakeConsumerManagementService : public ConsumerManagementService {
 public:
  FakeConsumerManagementService();
  virtual ~FakeConsumerManagementService();

  // Set both status and enrollment stage.
  void SetStatusAndEnrollmentStage(Status status, EnrollmentStage stage);

  // ConsumerManagementServcie:
  virtual Status GetStatus() const override;
  virtual EnrollmentStage GetEnrollmentStage() const override;
  virtual void SetEnrollmentStage(EnrollmentStage stage) override;

 private:
  Status status_;
  EnrollmentStage enrollment_stage_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_
