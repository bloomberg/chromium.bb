// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/background_fetch/background_fetch_embedded_worker_test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace content {

class BackgroundFetchRegistrationId;
class ServiceWorkerRegistration;

// Base class containing common functionality needed in unit tests written for
// the Background Fetch feature.
class BackgroundFetchTestBase : public ::testing::Test {
 public:
  BackgroundFetchTestBase();
  ~BackgroundFetchTestBase() override;

  // ::testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

  // Creates a valid Service Worker registration for the testing origin and
  // stores the data in the |*registration_id|. Returns whether creation was
  // successful, which must be asserted by tests.
  bool CreateRegistrationId(const std::string& tag,
                            BackgroundFetchRegistrationId* registration_id)
      WARN_UNUSED_RESULT;

  // Returns the embedded worker test helper instance, which can be used to
  // influence the behaviour of the Service Worker events.
  BackgroundFetchEmbeddedWorkerTestHelper* embedded_worker_test_helper() {
    return &embedded_worker_test_helper_;
  }

  // Returns the origin that should be used for Background Fetch tests.
  const url::Origin& origin() const { return origin_; }

 private:
  TestBrowserThreadBundle thread_bundle_;

  BackgroundFetchEmbeddedWorkerTestHelper embedded_worker_test_helper_;

  url::Origin origin_;

  // Vector of ServiceWorkerRegistration instances that have to be kept alive
  // for the lifetime of this test.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      service_worker_registrations_;

  bool set_up_called_ = false;
  bool tear_down_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchTestBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_
