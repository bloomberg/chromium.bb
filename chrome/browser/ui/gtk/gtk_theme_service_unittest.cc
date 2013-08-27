// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/skia_utils_gtk.h"

class GtkThemeServiceTest : public testing::Test {
 public:
  GtkThemeServiceTest() : provider_(NULL) {}

  void SetUseGtkTheme(bool use_gtk_theme) {
    profile_.GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, use_gtk_theme);
  }

  void BuildProvider() {
    provider_ = GtkThemeService::GetFrom(&profile_);
  }

 protected:
  TestingProfile profile_;

  GtkThemeService* provider_;
};

TEST_F(GtkThemeServiceTest, DefaultValues) {
  SetUseGtkTheme(false);
  BuildProvider();

  // Test that we get the default theme colors back when in normal mode.
  for (int i = ThemeProperties::COLOR_FRAME;
       i <= ThemeProperties::COLOR_BUTTON_BACKGROUND; ++i) {
    EXPECT_EQ(provider_->GetColor(i), ThemeProperties::GetDefaultColor(i))
        << "Wrong default color for " << i;
  }
}

TEST_F(GtkThemeServiceTest, UsingGtkValues) {
  SetUseGtkTheme(true);
  BuildProvider();

  // This test only verifies that we're using GTK values. Because of Gtk's
  // large, implied global state, it would take some IN_PROCESS_BROWSER_TESTS
  // to write an equivalent of DefaultValues above in a way that wouldn't make
  // other tests flaky. kColorTabText is the only simple path where there's no
  // weird calculations for edge cases so use that as a simple test.
  GtkWidget* fake_label = provider_->fake_label();
  GtkStyle* label_style = gtk_rc_get_style(fake_label);
  GdkColor label_color = label_style->fg[GTK_STATE_NORMAL];
  EXPECT_EQ(provider_->GetColor(ThemeProperties::COLOR_TAB_TEXT),
            gfx::GdkColorToSkColor(label_color));
}
