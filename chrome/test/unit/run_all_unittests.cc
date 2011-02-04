// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/unit/chrome_test_suite.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"

namespace {

// A stubbed out webkit client impl.
class UnitTestWebKitClient : public WebKit::WebKitClient {
 public:
  UnitTestWebKitClient() {
  }
};

// A special test suite that also initializes webkit once for all unittests.
// This is useful for two reasons:
// 1. It allows the use of some primitive webkit data types like WebString.
// 2. Individual unittests should not be initting webkit on their own, initting
// it here ensures attempts to do so within an individual test will fail.
class UnitTestTestSuite : public ChromeTestSuite {
 public:
  UnitTestTestSuite(int argc, char** argv)
      : ChromeTestSuite(argc, argv) {
  }

 protected:
  virtual void Initialize() {
    WebKit::initialize(&webkitclient_);
    ChromeTestSuite::Initialize();
  }
  virtual void Shutdown() {
    ChromeTestSuite::Shutdown();
    WebKit::shutdown();
  }

  UnitTestWebKitClient webkitclient_;
};

}  // namespace

int main(int argc, char **argv) {
  return UnitTestTestSuite(argc, argv).Run();
}
