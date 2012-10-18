// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/test/device_test_suite.h"

#include "content/public/common/content_client.h"

DeviceTestSuite::DeviceTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {
}

DeviceTestSuite::~DeviceTestSuite() {
}

content::ContentClient* DeviceTestSuite::CreateClientForInitialization() {
  return new content::ContentClient();
}
