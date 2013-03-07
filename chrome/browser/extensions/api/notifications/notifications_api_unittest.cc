// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace extensions {

class ExtensionNotificationsTest : public BrowserWithTestWindowTest {
};

TEST_F(ExtensionNotificationsTest, Channels) {
  scoped_refptr<Extension> extension(utils::CreateEmptyExtensionWithLocation(
        extensions::Manifest::UNPACKED));
  scoped_refptr<NotificationsClearFunction> notification_function(
      new extensions::NotificationsClearFunction());
  notification_function->set_extension(extension.get());
  {
    Feature::ScopedCurrentChannel channel_scope(
          chrome::VersionInfo::CHANNEL_CANARY);
    ASSERT_TRUE(notification_function->IsNotificationsApiAvailable());
  }
  {
    Feature::ScopedCurrentChannel channel_scope(
          chrome::VersionInfo::CHANNEL_DEV);
    ASSERT_TRUE(notification_function->IsNotificationsApiAvailable());
  }
  {
    Feature::ScopedCurrentChannel channel_scope(
          chrome::VersionInfo::CHANNEL_BETA);
    ASSERT_FALSE(notification_function->IsNotificationsApiAvailable());
  }
  {
    Feature::ScopedCurrentChannel channel_scope(
          chrome::VersionInfo::CHANNEL_STABLE);
    ASSERT_FALSE(notification_function->IsNotificationsApiAvailable());
  }
}

}  // namespace extensions
