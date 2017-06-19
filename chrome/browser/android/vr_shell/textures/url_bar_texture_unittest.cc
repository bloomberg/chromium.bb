// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/vr_shell/textures/render_text_wrapper.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/render_text.h"

using security_state::SecurityLevel;

namespace vr_shell {

// TODO(cjgrant): Use ColorScheme instead of hardcoded values
// where it makes sense.
static const SkColor kEmphasizedColor = 0xFF000000;
static const SkColor kDeemphasizedColor = 0xFF5A5A5A;
static const SkColor kSecureColor = gfx::kGoogleGreen700;
static const SkColor kWarningColor = gfx::kGoogleRed700;

static const SkColor kIncognitoDeemphasizedColor = 0xFF878787;
static const SkColor kIncognitoEmphasizedColor = 0xFFEDEDED;
static const SkColor kIncognitoSecureColor = 0xFFEDEDED;
static const SkColor kIncognitoWarningColor = 0xFFEDEDED;

static constexpr int kUrlWidth = 400;
static constexpr int kUrlHeight = 30;

class TestUrlBarTexture : public UrlBarTexture {
 public:
  TestUrlBarTexture();
  ~TestUrlBarTexture() override {}

  void DrawURL(const GURL& gurl) {
    unsupported_mode_ = UiUnsupportedMode::kCount;
    SetURL(gurl);
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        texture_size_.width(), texture_size_.height());
    DrawAndLayout(surface->getCanvas(), texture_size_);
  }

  void SetForceFontFallbackFailure(bool force) {
    SetForceFontFallbackFailureForTesting(force);
  }

  size_t GetNumberOfFontFallbacksForURL(const GURL& gurl) {
    url::Parsed parsed;
    const base::string16 text = url_formatter::FormatUrl(
        gurl, url_formatter::kFormatUrlOmitAll, net::UnescapeRule::NORMAL,
        &parsed, nullptr, nullptr);

    gfx::FontList font_list;
    if (!GetFontList(kUrlHeight, text, &font_list))
      return 0;

    return font_list.GetFonts().size();
  }

  // Reports the last unsupported mode that was encountered. Returns kCount if
  // no unsupported mode was encountered.
  UiUnsupportedMode unsupported_mode() const { return unsupported_mode_; }

 private:
  void OnUnsupportedFeature(UiUnsupportedMode mode) {
    unsupported_mode_ = mode;
  }

  gfx::Size texture_size_;
  gfx::Rect bounds_;
  UiUnsupportedMode unsupported_mode_ = UiUnsupportedMode::kCount;
};

TestUrlBarTexture::TestUrlBarTexture()
    : UrlBarTexture(false,
                    base::Bind(&TestUrlBarTexture::OnUnsupportedFeature,
                               base::Unretained(this))),
      texture_size_(kUrlWidth, kUrlHeight),
      bounds_(kUrlWidth, kUrlHeight) {
  gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
}

class MockRenderText : public RenderTextWrapper {
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
    UrlBarTexture::ApplyUrlStyling(
        formatted_url, parsed, level, &mock_,
        ColorScheme::GetColorScheme(ColorScheme::kModeNormal));
    UrlBarTexture::ApplyUrlStyling(
        formatted_url, parsed, level, &mock_,
        ColorScheme::GetColorScheme(ColorScheme::kModeIncognito));
  }

  testing::InSequence in_sequence_;
  MockRenderText mock_;
};

#if !defined(OS_LINUX)
// TODO(crbug/731894): This test does not work on Linux.
TEST(UrlBarTextureTest, WillNotFailOnNonAsciiURLs) {
  TestUrlBarTexture texture;
  EXPECT_EQ(3lu, texture.GetNumberOfFontFallbacksForURL(
                     GURL("http://中央大学.ಠ_ಠ.tw/")));
}
#endif

TEST_F(UrlEmphasisTest, SecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kSecureColor, gfx::Range(0, 5)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoSecureColor, gfx::Range(0, 5)));
  Verify("https://host.com/page", SecurityLevel::SECURE,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(8, 16)));
  Verify("https://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 8)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(0, 8)));
  Verify("http://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "host.com/page");
}

TEST_F(UrlEmphasisTest, DangerousHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kWarningColor, gfx::Range(0, 5)));
  EXPECT_CALL(mock_,
              ApplyStyle(gfx::TextStyle::STRIKE, true, gfx::Range(0, 5)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoWarningColor, gfx::Range(0, 5)));
  EXPECT_CALL(mock_,
              ApplyStyle(gfx::TextStyle::STRIKE, true, gfx::Range(0, 5)));
  Verify("https://host.com/page", SecurityLevel::DANGEROUS,
         "https://host.com/page");
}

TEST_F(UrlEmphasisTest, Data) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 4)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(0, 4)));
  Verify("data:text/html,lots of data", SecurityLevel::NONE,
         "data:text/html,lots of data");
}

TEST(UrlBarTextureTest, WillFailOnUnhandledCodePoint) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL("https://foo.com"));
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
  texture.SetForceFontFallbackFailure(true);
  texture.DrawURL(GURL("https://bar.com"));
  EXPECT_EQ(UiUnsupportedMode::kUnhandledCodePoint, texture.unsupported_mode());
  texture.SetForceFontFallbackFailure(false);
  texture.DrawURL(GURL("https://baz.com"));
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
}

TEST(UrlBarTextureTest, WillFailOnStrongRTLChar) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL("https://ש.com"));
  EXPECT_EQ(UiUnsupportedMode::kURLWithStrongRTLChars,
            texture.unsupported_mode());
}

TEST(UrlBarTexture, ElisionIsAnUnsupportedMode) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL(
      "https://"
      "thereisnopossiblewaythatthishostnamecouldbecontainedinthelimitedspacetha"
      "tweareaffordedtousitsreallynotsomethingweshouldconsiderorplanfororpinour"
      "hopesonlestwegetdisappointedor.sad.com"));
  EXPECT_EQ(UiUnsupportedMode::kCouldNotElideURL, texture.unsupported_mode());
}

TEST(UrlBarTexture, ShortURLAreIndeedSupported) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL("https://short.com/"));
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
}

TEST(UrlBarTexture, LongPathsDoNotRequireElisionAndAreSupported) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL(
      "https://something.com/"
      "thereisnopossiblewaythatthishostnamecouldbecontainedinthelimitedspacetha"
      "tweareaffordedtousitsreallynotsomethingweshouldconsiderorplanfororpinour"
      "hopesonlestwegetdisappointedorsad.com"));
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
}

}  // namespace vr_shell
