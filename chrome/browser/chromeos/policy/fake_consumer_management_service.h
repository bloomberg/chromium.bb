// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/consumer_management_stage.h"

namespace policy {

class FakeConsumerManagementService : public ConsumerManagementService {
 public:
  FakeConsumerManagementService();
  ~FakeConsumerManagementService() override;

  // Set both status and stage.
  void SetStatusAndStage(Status status, const ConsumerManagementStage& stage);

  // ConsumerManagementServcie:
  Status GetStatus() const override;
  ConsumerManagementStage GetStage() const override;
  void SetStage(const ConsumerManagementStage& stage) override;

 private:
  Status status_;
  ConsumerManagementStage stage_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsumerManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_FAKE_CONSUMER_MANAGEMENT_SERVICE_H_
