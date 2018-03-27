// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar_texture.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "chrome/browser/vr/elements/render_text_wrapper.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/test/mock_render_text.h"
#include "components/security_state/core/security_state.h"
#include "components/toolbar/vector_icons.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/render_text.h"

using security_state::SecurityLevel;

namespace vr {

constexpr SkColor kEmphasizedColor = 0x01010101;
constexpr SkColor kDeemphasizedColor = 0x02020202;

constexpr int kUrlWidthPixels = 1024;

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
                             vr::RenderTextWrapper* render_text) {
    UrlTextColors colors;
    colors.deemphasized = kDeemphasizedColor;
    colors.emphasized = kEmphasizedColor;
    ApplyUrlStyling(formatted_url, parsed, render_text, colors);
  }

  void SetForceFontFallbackFailure(bool force) {
    SetForceFontFallbackFailureForTesting(force);
  }

  size_t GetNumberOfFontFallbacksForURL(const GURL& gurl) {
    url::Parsed parsed;
    const base::string16 text = url_formatter::FormatUrl(
        gurl, GetVrFormatUrlTypes(), net::UnescapeRule::NORMAL, &parsed,
        nullptr, nullptr);

    gfx::Size texture_size = GetPreferredTextureSize(kUrlWidthPixels);
    gfx::FontList font_list;
    if (!GetDefaultFontList(texture_size.height(), text, &font_list))
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

  UiUnsupportedMode unsupported_mode_ = UiUnsupportedMode::kCount;
};

TestUrlBarTexture::TestUrlBarTexture()
    : UrlBarTexture(
          base::BindRepeating(&TestUrlBarTexture::OnUnsupportedFeature,
                              base::Unretained(this))) {
  gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");

  UrlTextColors colors;
  colors.deemphasized = kDeemphasizedColor;
  colors.emphasized = kEmphasizedColor;
  SetColors(colors);
  SetBackgroundColor(SK_ColorBLACK);
  SetForegroundColor(SK_ColorWHITE);
}

class UrlEmphasisTest : public testing::Test {
 protected:
  void Verify(const std::string& url_string,
              SecurityLevel level,
              const std::string& expected_string) {
    GURL url(base::UTF8ToUTF16(url_string));
    url::Parsed parsed;
    const base::string16 formatted_url = url_formatter::FormatUrl(
        url, GetVrFormatUrlTypes(), net::UnescapeRule::NORMAL, &parsed, nullptr,
        nullptr);
    EXPECT_EQ(formatted_url, base::UTF8ToUTF16(expected_string));
    TestUrlBarTexture::TestUrlStyling(formatted_url, parsed, level, &mock_);
  }

  testing::InSequence in_sequence_;
  MockRenderText mock_;
};

#if !defined(OS_LINUX) && !defined(OS_WIN)
// TODO(crbug/731894): This test does not work on Linux.
// TODO(crbug/770893): This test does not work on Windows.
TEST(UrlBarTextureTest, WillNotFailOnNonAsciiURLs) {
  TestUrlBarTexture texture;
  EXPECT_EQ(3lu, texture.GetNumberOfFontFallbacksForURL(
                     GURL("http://中央大学.ಠ_ಠ.tw/")));
}
#endif

TEST_F(UrlEmphasisTest, SecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 8)));
  Verify("https://host.com/page", SecurityLevel::SECURE, "host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpsHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 8)));
  Verify("https://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "host.com/page");
}

TEST_F(UrlEmphasisTest, NotSecureHttpHost) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 8)));
  Verify("http://host.com/page", SecurityLevel::HTTP_SHOW_WARNING,
         "host.com/page");
}

TEST_F(UrlEmphasisTest, Data) {
  EXPECT_CALL(mock_, SetColor(kDeemphasizedColor));
  EXPECT_CALL(mock_, ApplyColor(kEmphasizedColor, gfx::Range(0, 4)));
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

}  // namespace vr
