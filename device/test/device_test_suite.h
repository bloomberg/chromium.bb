// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_TEST_DEVICE_TEST_SUITE_H_
#define DEVICE_TEST_DEVICE_TEST_SUITE_H_

#include "content/public/test/content_test_suite_base.h"

class DeviceTestSuite : public content::ContentTestSuiteBase {
 public:
  DeviceTestSuite(int argc, char** argv);
  virtual ~DeviceTestSuite();

 protected:
  virtual content::ContentClient* CreateClientForInitialization() OVERRIDE;
};

#endif  // DEVICE_TEST_DEVICE_TEST_SUITE_H_
