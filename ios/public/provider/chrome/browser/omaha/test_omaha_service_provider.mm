// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/omaha/test_omaha_service_provider.h"

TestOmahaServiceProvider::TestOmahaServiceProvider() {}

TestOmahaServiceProvider::~TestOmahaServiceProvider() {}

GURL TestOmahaServiceProvider::GetUpdateServerURL() const {
  return GURL();
}

std::string TestOmahaServiceProvider::GetApplicationID() const {
  return std::string();
}

std::string TestOmahaServiceProvider::GetBrandCode() const {
  return std::string();
}

void TestOmahaServiceProvider::AppendExtraAttributes(
    const std::string& tag,
    OmahaXmlWriter* writer) const {}
