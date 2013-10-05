// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/client_hints.h"

#include <locale.h>

#include "base/logging.h"
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_IOS)
#include "base/test/scoped_locale.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

class ClientHintsTest : public testing::Test {
 public:
  void UpdateScreenInfo(float pixel_ratio) {
    client_hints_.UpdateScreenInfo(pixel_ratio);
  };

 protected:
  ClientHints client_hints_;
};

TEST_F(ClientHintsTest, HintsWellFormatted) {
  UpdateScreenInfo(1.567f);
  std::string hint = client_hints_.GetDevicePixelRatioHeader();
  EXPECT_EQ("1.57", hint);
}

// Android and iOS do not support setLocale.
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_IOS)
// TODO(bengr): Use ScopedLocal in Windows once it is supported.
TEST_F(ClientHintsTest, HintsWellFormattedWithNonEnLocale) {
  base::ScopedLocale locale("fr_FR.UTF-8");
  UpdateScreenInfo(1.567f);
  std::string hint = client_hints_.GetDevicePixelRatioHeader();
  EXPECT_EQ("1.57", hint);
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_IOS)

TEST_F(ClientHintsTest, HintsHaveNonbogusValues) {
  UpdateScreenInfo(-1.567f);
  std::string hint = client_hints_.GetDevicePixelRatioHeader();
  EXPECT_EQ("", hint);

  UpdateScreenInfo(1.567f);
  hint = client_hints_.GetDevicePixelRatioHeader();
  EXPECT_EQ("1.57", hint);

  UpdateScreenInfo(0.0f);
  hint = client_hints_.GetDevicePixelRatioHeader();
  // Hints should be last known good values.
  EXPECT_EQ("1.57", hint);
}

