// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/invalidation_service_android.h"

#include "chrome/browser/invalidation/invalidation_frontend_test_template.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class InvalidationServiceAndroidTestDelegate {
 public:
  InvalidationServiceAndroidTestDelegate() { }

  ~InvalidationServiceAndroidTestDelegate() {
    DestroyInvalidationFrontend();
  }

  void CreateInvalidationFrontend() {
    profile_.reset(new TestingProfile());
    invalidation_service_android_.reset(
        new InvalidationServiceAndroid(profile_.get()));
  }

  InvalidationFrontend* GetInvalidationFrontend() {
    return invalidation_service_android_.get();
  }

  void DestroyInvalidationFrontend() {
    invalidation_service_android_->Shutdown();
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    invalidation_service_android_->TriggerStateChangeForTest(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    syncer::ModelTypeInvalidationMap model_invalidation_map =
            ObjectIdInvalidationMapToModelTypeInvalidationMap(invalidation_map);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
        content::Source<Profile>(profile_.get()),
        content::Details<const syncer::ModelTypeInvalidationMap>(
            &model_invalidation_map));
  }

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<InvalidationServiceAndroid> invalidation_service_android_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    AndroidInvalidationServiceTest, InvalidationFrontendTest,
    InvalidationServiceAndroidTestDelegate);

}  // namespace invalidation
