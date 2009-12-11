// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Converts a GdkColor to a SkColor.
SkColor GdkToSkColor(GdkColor* color) {
  return SkColorSetRGB(color->red >> 8,
                       color->green >> 8,
                       color->blue >> 8);
}

}  // namespace

class GtkThemeProviderTest : public testing::Test {
 public:
  GtkThemeProviderTest() : provider_(NULL) {}

  void SetUseGtkTheme(bool use_gtk_theme) {
    profile_.GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, use_gtk_theme);
  }

  void BuildProvider() {
    profile_.InitThemes();
    provider_ = GtkThemeProvider::GetFrom(&profile_);
  }

  void UseThemeProvider(GtkThemeProvider* provider) {
    profile_.UseThemeProvider(provider);
    provider_ = GtkThemeProvider::GetFrom(&profile_);
  }

 protected:
  TestingProfile profile_;

  GtkThemeProvider* provider_;
};

TEST_F(GtkThemeProviderTest, DefaultValues) {
  SetUseGtkTheme(false);
  BuildProvider();

  // Test that we get the default theme colors back when in normal mode.
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_FRAME),
            BrowserThemeProvider::kDefaultColorFrame);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_FRAME_INACTIVE),
            BrowserThemeProvider::kDefaultColorFrameInactive);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_FRAME_INCOGNITO),
            BrowserThemeProvider::kDefaultColorFrameIncognito);
  EXPECT_EQ(provider_->GetColor(
      BrowserThemeProvider::COLOR_FRAME_INCOGNITO_INACTIVE),
      BrowserThemeProvider::kDefaultColorFrameIncognitoInactive);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_TOOLBAR),
            BrowserThemeProvider::kDefaultColorToolbar);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_TAB_TEXT),
            BrowserThemeProvider::kDefaultColorTabText);
  EXPECT_EQ(provider_->GetColor(
      BrowserThemeProvider::COLOR_BACKGROUND_TAB_TEXT),
      BrowserThemeProvider::kDefaultColorBackgroundTabText);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT),
            BrowserThemeProvider::kDefaultColorBookmarkText);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND),
            BrowserThemeProvider::kDefaultColorNTPBackground);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_TEXT),
            BrowserThemeProvider::kDefaultColorNTPText);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_LINK),
            BrowserThemeProvider::kDefaultColorNTPLink);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_HEADER),
            BrowserThemeProvider::kDefaultColorNTPHeader);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION),
            BrowserThemeProvider::kDefaultColorNTPSection);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_TEXT),
            BrowserThemeProvider::kDefaultColorNTPSectionText);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_NTP_SECTION_LINK),
            BrowserThemeProvider::kDefaultColorNTPSectionLink);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_CONTROL_BACKGROUND),
            BrowserThemeProvider::kDefaultColorControlBackground);
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND),
            BrowserThemeProvider::kDefaultColorButtonBackground);
}

TEST_F(GtkThemeProviderTest, UsingGtkValues) {
  SetUseGtkTheme(true);
  BuildProvider();

  // This test only verifies that we're using GTK values. Because of Gtk's
  // large, implied global state, it would take some IN_PROCESS_BROWSER_TESTS
  // to write an equivalent of DefaultValues above in a way that wouldn't make
  // other tests flaky. kColorTabText is the only simple path where there's no
  // weird calculations for edge cases so use that as a simple test.
  GtkWidget* fake_label = provider_->fake_label();
  GtkStyle* label_style = gtk_rc_get_style(fake_label);
  GdkColor label_color = label_style->text[GTK_STATE_NORMAL];
  EXPECT_EQ(provider_->GetColor(BrowserThemeProvider::COLOR_TAB_TEXT),
            GdkToSkColor(&label_color));
}

// Helper class to GtkThemeProviderTest.UsingGtkFrame.
class ImageVerifierGtkThemeProvider : public GtkThemeProvider {
 public:
  ImageVerifierGtkThemeProvider() : theme_toolbar_(NULL) { }

  virtual SkBitmap* LoadThemeBitmap(int id) const {
    if (id != IDR_THEME_TOOLBAR)
      return GtkThemeProvider::LoadThemeBitmap(id);
    theme_toolbar_ = GtkThemeProvider::LoadThemeBitmap(id);
    return theme_toolbar_;
  }

  mutable SkBitmap* theme_toolbar_;
};

TEST_F(GtkThemeProviderTest, InjectsToolbar) {
  SetUseGtkTheme(true);
  ImageVerifierGtkThemeProvider* verifier_provider =
      new ImageVerifierGtkThemeProvider;
  UseThemeProvider(verifier_provider);

  // Make sure the image we get from the public BrowserThemeProvider interface
  // is the one we injected through GtkThemeProvider.
  SkBitmap* image = provider_->GetBitmapNamed(IDR_THEME_TOOLBAR);
  EXPECT_TRUE(verifier_provider->theme_toolbar_);
  EXPECT_TRUE(image);
  EXPECT_EQ(verifier_provider->theme_toolbar_, image);
}
