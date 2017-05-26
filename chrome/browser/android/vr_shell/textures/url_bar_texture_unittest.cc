// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/vr_shell/textures/render_text_wrapper.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/render_text.h"

using security_state::SecurityLevel;

namespace vr_shell {

class MockRenderText : public vr_shell::RenderTextWrapper {
 public:
  MockRenderText() : RenderTextWrapper(nullptr) {}
  ~MockRenderText() override {}

  MOCK_METHOD1(SetColor, void(SkColor value));
  MOCK_METHOD2(ApplyColor, void(SkColor value, const gfx::Range& range));
  MOCK_METHOD2(SetStyle, void(gfx::TextStyle style, bool value));
  MOCK_METHOD3(ApplyStyle,
               void(gfx::TextStyle style, bool value, const gfx::Range& range));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderText);
};

static constexpr SkColor kEmphasizedColor = 0xFF000000;
static constexpr SkColor kDeemphasizedColor = 0xFF5A5A5A;
static const SkColor kSecureColor = gfx::kGoogleGreen700;
static const SkColor kWarningColor = gfx::kGoogleRed700;

class UrlEmphasisTest : public testing::Test {
 protected:
  void Verify(const std::string& url_string,
              SecurityLevel level,
              const std::string& expected_string) {
    GURL url(base::UTF8ToUTF16(url_string));
    url::Parsed parsed;
    const base::string16 formatted_url = url_formatter::FormatUrl(
        url, url_formatter::kFormatUrlOmitAll, net::UnescapeRule::NORMAL,
        &parsed, nullptr, nullptr);
    EXPECT_EQ(formatted_url, base::UTF8ToUTF16(expected_string));
    UrlBarTexture::ApplyUrlStyling(formatted_url, parsed, level, &mock_);
  }

  testing::InSequence in_sequence_;
  MockRenderText mock_;
};

TEST_F(UrlEmphasisTest, SecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kSecureColor, gfx::Range(0, 5)));
  Verify("https://host.com/page", SecurityLevel::SECURE,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  Verify("https://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 8)));
  Verify("http://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "host.com/page");
}

TEST_F(UrlEmphasisTest, DangerousHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kWarningColor, gfx::Range(0, 5)));
  EXPECT_CALL(mock_, ApplyStyle(gfx::TextStyle::DIAGONAL_STRIKE, true,
                                gfx::Range(0, 5)));
  Verify("https://host.com/page", SecurityLevel::DANGEROUS,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, Data) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 4)));
  Verify("data:text/html,lots of data", SecurityLevel::NONE,
         "data:text/html,lots of data");
}

}  // namespace vr_shell
