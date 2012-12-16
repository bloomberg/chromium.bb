// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/pepper_flash_content_settings_utils.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

using options::MediaException;
using options::MediaExceptions;
using options::PepperFlashContentSettingsUtils;

namespace {

MediaExceptions ConvertAndSort(const MediaException* items, size_t count) {
  MediaExceptions result(items, items + count);
  PepperFlashContentSettingsUtils::SortMediaExceptions(&result);
  return result;
}

}  // namespace

TEST(PepperFlashContentSettingsUtilsTest, SortMediaExceptions) {
  MediaException entry_1(ContentSettingsPattern::FromString("www.google.com"),
                         CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK);
  MediaException entry_2(ContentSettingsPattern::FromString("www.youtube.com"),
                         CONTENT_SETTING_BLOCK, CONTENT_SETTING_DEFAULT);
  MediaException entry_3(ContentSettingsPattern::Wildcard(),
                         CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK);
  MediaException entry_4(ContentSettingsPattern(),
                         CONTENT_SETTING_SESSION_ONLY, CONTENT_SETTING_ALLOW);

  MediaExceptions list_1;
  list_1.push_back(entry_1);
  list_1.push_back(entry_2);
  list_1.push_back(entry_3);
  list_1.push_back(entry_4);

  MediaExceptions list_2;
  list_2.push_back(entry_1);
  list_2.push_back(entry_3);
  list_2.push_back(entry_2);
  list_2.push_back(entry_4);

  MediaExceptions list_3;
  list_3.push_back(entry_4);
  list_3.push_back(entry_1);
  list_3.push_back(entry_2);
  list_3.push_back(entry_3);

  EXPECT_NE(list_1, list_2);
  EXPECT_NE(list_2, list_3);
  EXPECT_NE(list_3, list_1);

  PepperFlashContentSettingsUtils::SortMediaExceptions(&list_1);
  PepperFlashContentSettingsUtils::SortMediaExceptions(&list_2);
  PepperFlashContentSettingsUtils::SortMediaExceptions(&list_3);

  EXPECT_EQ(list_1, list_2);
  EXPECT_EQ(list_2, list_3);
}

TEST(PepperFlashContentSettingsUtilsTest, AreMediaExceptionsEqual) {
  {
    // Empty lists are equal.
    // Default settings are not compared directly, so it is possible to return
    // true when they are different.
    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_BLOCK,
        MediaExceptions(),
        CONTENT_SETTING_ASK,
        MediaExceptions(),
        false,
        false));
  }

  {
    MediaException exceptions_1[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW),
      MediaException(ContentSettingsPattern::FromString("www.youtube.com"),
                     CONTENT_SETTING_ASK, CONTENT_SETTING_ASK)
    };

    MediaException exceptions_2[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW)
    };

    // The exception of "www.youtube.com" in |exceptions_1| should not affect
    // the result, because it has the same settings as |default_setting_2|.
    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        false,
        false));
    // Changing |default_setting_2| should change the result.
    EXPECT_FALSE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
  }

  {
    // Similar to the previous block, but reoder the exceptions. The outcome
    // should be the same.
    MediaException exceptions_1[] = {
      MediaException(ContentSettingsPattern::FromString("www.youtube.com"),
                     CONTENT_SETTING_ASK, CONTENT_SETTING_ASK),
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW)
    };

    MediaException exceptions_2[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW)
    };

    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
    EXPECT_FALSE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ALLOW,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
  }

  {
    MediaException exceptions_1[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK)
    };

    MediaException exceptions_2[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW)
    };

    // Test that |ignore_video_setting| works.
    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        true));
    EXPECT_FALSE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
  }

  {
    MediaException exceptions_1[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW)
    };

    MediaException exceptions_2[] = {
      MediaException(ContentSettingsPattern::FromString("www.google.com"),
                     CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW)
    };

    // Test that |ignore_audio_setting| works.
    EXPECT_TRUE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        true,
        false));
    EXPECT_FALSE(PepperFlashContentSettingsUtils::AreMediaExceptionsEqual(
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_1, arraysize(exceptions_1)),
        CONTENT_SETTING_ASK,
        ConvertAndSort(exceptions_2, arraysize(exceptions_2)),
        false,
        false));
  }
}
