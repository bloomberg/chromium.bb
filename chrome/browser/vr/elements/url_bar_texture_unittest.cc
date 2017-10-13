// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar_texture.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "components/security_state/core/security_state.h"
#include "components/toolbar/vector_icons.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/render_text.h"

using security_state::SecurityLevel;

namespace vr {

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

static constexpr int kUrlWidthPixels = 1024;

class TestUrlBarTexture : public UrlBarTexture {
 public:
  TestUrlBarTexture();
  ~TestUrlBarTexture() override {}

  void DrawURL(const GURL& gurl) {
    ToolbarState state(gurl, SecurityLevel::DANGEROUS,
                       &toolbar::kHttpsInvalidIcon,
                       base::UTF8ToUTF16("Not secure"), true, false);
    ASSERT_TRUE(state.should_display_url);
    DrawURLState(state);
  }

  void DrawURLState(const ToolbarState& state) {
    unsupported_mode_ = UiUnsupportedMode::kCount;
    SetToolbarState(state);
    gfx::Size texture_size = GetPreferredTextureSize(kUrlWidthPixels);
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(
        texture_size.width(), texture_size.height());
    DrawAndLayout(surface->getCanvas(), texture_size);
  }

  static void TestUrlStyling(const base::string16& formatted_url,
                             const url::Parsed& parsed,
                             security_state::SecurityLevel security_level,
                             vr::RenderTextWrapper* render_text,
                             const ColorScheme& color_scheme) {
    ApplyUrlStyling(formatted_url, parsed, security_level, render_text,
                    color_scheme);
  }

  void SetForceFontFallbackFailure(bool force) {
    SetForceFontFallbackFailureForTesting(force);
  }

  size_t GetNumberOfFontFallbacksForURL(const GURL& gurl) {
    url::Parsed parsed;
    const base::string16 text = url_formatter::FormatUrl(
        gurl, url_formatter::kFormatUrlOmitDefaults, net::UnescapeRule::NORMAL,
        &parsed, nullptr, nullptr);

    gfx::Size texture_size = GetPreferredTextureSize(kUrlWidthPixels);
    gfx::FontList font_list;
    if (!GetFontList(texture_size.height(), text, &font_list))
      return 0;

    return font_list.GetFonts().size();
  }

  // Reports the last unsupported mode that was encountered. Returns kCount if
  // no unsupported mode was encountered.
  UiUnsupportedMode unsupported_mode() const { return unsupported_mode_; }

  gfx::RenderText* url_render_text() { return url_render_text_.get(); }

  const base::string16& url_text() { return rendered_url_text_; }
  const base::string16& security_text() { return rendered_security_text_; }
  const gfx::Rect url_rect() { return rendered_url_text_rect_; }
  const gfx::Rect security_rect() { return rendered_security_text_rect_; }

 private:
  void OnUnsupportedFeature(UiUnsupportedMode mode) {
    unsupported_mode_ = mode;
  }

  UiUnsupportedMode unsupported_mode_ = UiUnsupportedMode::kCount;
};

TestUrlBarTexture::TestUrlBarTexture()
    : UrlBarTexture(base::Bind(&TestUrlBarTexture::OnUnsupportedFeature,
                               base::Unretained(this))) {
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
  MOCK_METHOD1(SetStrikeThicknessFactor, void(SkScalar));

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
        url, url_formatter::kFormatUrlOmitDefaults, net::UnescapeRule::NORMAL,
        &parsed, nullptr, nullptr);
    EXPECT_EQ(formatted_url, base::UTF8ToUTF16(expected_string));
    TestUrlBarTexture::TestUrlStyling(
        formatted_url, parsed, level, &mock_,
        ColorScheme::GetColorScheme(ColorScheme::kModeNormal));
    TestUrlBarTexture::TestUrlStyling(
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
  EXPECT_CALL(mock_, SetStrikeThicknessFactor(testing::_));
  EXPECT_CALL(mock_,
              ApplyStyle(gfx::TextStyle::STRIKE, true, gfx::Range(0, 5)));
  EXPECT_CALL(mock_, SetColor(kIncognitoDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoEmphasizedColor, gfx::Range(8, 16)));
  EXPECT_CALL(mock_, ApplyColor(kIncognitoWarningColor, gfx::Range(0, 5)));
  EXPECT_CALL(mock_, SetStrikeThicknessFactor(testing::_));
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

TEST(UrlBarTextureTest, MaliciousRTLIsRenderedLTR) {
  TestUrlBarTexture texture;

  // Construct a malicious URL that attempts to spoof the hostname.
  const std::string real_host("127.0.0.1");
  const std::string spoofed_host("attack.com");
  const std::string url =
      "http://" + real_host + "/ا/http://" + spoofed_host + "‬";

  texture.DrawURL(GURL(base::UTF8ToUTF16(url)));

  // Determine the logical character ranges of the legitimate and spoofed
  // hostnames in the processed URL text.
  base::string16 text = texture.url_text();
  base::string16 real_16 = base::UTF8ToUTF16(real_host);
  base::string16 spoofed_16 = base::UTF8ToUTF16(spoofed_host);
  size_t position = text.find(real_16);
  ASSERT_NE(position, base::string16::npos);
  gfx::Range real_range(position, position + real_16.size());
  position = text.find(spoofed_16);
  ASSERT_NE(position, base::string16::npos);
  gfx::Range spoofed_range(position, position + spoofed_16.size());

  // Extract the pixel locations at which hostnames were actually rendered.
  auto real_bounds =
      texture.url_render_text()->GetSubstringBoundsForTesting(real_range);
  auto spoofed_bounds =
      texture.url_render_text()->GetSubstringBoundsForTesting(spoofed_range);
  EXPECT_EQ(real_bounds.size(), 1u);
  EXPECT_GE(spoofed_bounds.size(), 1u);

  // Verify that any spoofed portion of the hostname has remained to the right
  // of the legitimate hostname. This will fail if LTR directionality is not
  // specified during URL rendering.
  auto minimum_position = real_bounds[0].x() + real_bounds[0].width();
  for (const auto& region : spoofed_bounds) {
    EXPECT_GT(region.x(), minimum_position);
  }
}

TEST(UrlBarTexture, ShortURLAreIndeedSupported) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL("https://short.com/"));
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
}

TEST(UrlBarTexture, EmptyURL) {
  TestUrlBarTexture texture;
  texture.DrawURL(GURL());
  EXPECT_EQ(UiUnsupportedMode::kCount, texture.unsupported_mode());
}

TEST(UrlBarTexture, OfflinePage) {
  TestUrlBarTexture texture;
  ToolbarState state(GURL("https://host.com/page"), SecurityLevel::NONE,
                     &toolbar::kHttpsInvalidIcon, base::UTF8ToUTF16("Offline"),
                     true, false);

  // Render online page.
  state.offline_page = false;
  texture.DrawURLState(state);
  EXPECT_EQ(texture.security_rect().width(), 0);
  EXPECT_EQ(texture.security_rect().height(), 0);
  EXPECT_GT(texture.url_rect().width(), 0);
  EXPECT_GT(texture.url_rect().height(), 0);
  EXPECT_TRUE(texture.security_text().empty());
  EXPECT_EQ(texture.url_text(), base::UTF8ToUTF16("https://host.com/page"));
  gfx::Rect online_url_rect = texture.url_rect();

  // Go offline. Security text should be visible and displace the URL.
  state.offline_page = true;
  texture.DrawURLState(state);
  EXPECT_GT(texture.security_rect().width(), 0);
  EXPECT_GT(texture.security_rect().height(), 0);
  EXPECT_GT(texture.url_rect().width(), 0);
  EXPECT_GT(texture.url_rect().height(), 0);
  EXPECT_GT(texture.url_rect().x(), online_url_rect.x());
  EXPECT_EQ(texture.security_text(), base::UTF8ToUTF16("Offline"));
  EXPECT_EQ(texture.url_text(), base::UTF8ToUTF16("host.com/page"));

  // Go back online.
  state.offline_page = false;
  texture.DrawURLState(state);
  EXPECT_EQ(texture.security_rect().width(), 0);
  EXPECT_EQ(texture.security_rect().height(), 0);
  EXPECT_EQ(texture.url_rect(), online_url_rect);
  EXPECT_TRUE(texture.security_text().empty());
  EXPECT_EQ(texture.url_text(), base::UTF8ToUTF16("https://host.com/page"));
}

}  // namespace vr
