// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_dark_aura.h"
#endif

#if defined(USE_X11)
#include "ui/views/linux_ui/linux_ui.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ash_config.h"
#endif

namespace {

enum PopupType { WIDE, NARROW, ROUNDED };

// A View that positions itself over another View to intercept clicks.
class ClickTrackingOverlayView : public views::View {
 public:
  ClickTrackingOverlayView(views::View* over, gfx::Point* last_click)
      : last_click_(last_click) {
    SetBoundsRect(over->bounds());
    over->parent()->AddChildView(this);
  }

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override {
    *last_click_ = event->location();
  }

 private:
  gfx::Point* last_click_;
};

// Helper to wait for theme changes. The wait is triggered when an instance of
// this class goes out of scope.
class ThemeChangeWaiter {
 public:
  explicit ThemeChangeWaiter(ThemeService* theme_service)
      : theme_change_observer_(chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                               content::Source<ThemeService>(theme_service)) {}

  ~ThemeChangeWaiter() {
    theme_change_observer_.Wait();
    // Theme changes propagate asynchronously in DesktopWindowTreeHostX11::
    // FrameTypeChanged(), so ensure all tasks are consumed.
    content::RunAllPendingInMessageLoop();
  }

 private:
  content::WindowedNotificationObserver theme_change_observer_;

  DISALLOW_COPY_AND_ASSIGN(ThemeChangeWaiter);
};

std::string PrintPopupType(const testing::TestParamInfo<PopupType>& info) {
  switch (info.param) {
    case WIDE:
      return "Wide";
    case NARROW:
      return "Narrow";
    case ROUNDED:
      return "Rounded";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

class OmniboxPopupContentsViewTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<PopupType> {
 public:
  OmniboxPopupContentsViewTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    switch (GetParam()) {
      case ROUNDED:
        // Set --top-chrome-md=material-touch-optimized to enable the ROUNDED
        // style (which is the only supported style in that mode).
        command_line->AppendSwitchASCII(
            switches::kTopChromeMD,
            switches::kTopChromeMDMaterialTouchOptimized);
        break;
      case NARROW:
        feature_list_.InitAndEnableFeature(
            omnibox::kUIExperimentNarrowDropdown);
        FALLTHROUGH;
      default:
        // Cater for the touch-optimized UI being enabled by default by always
        // setting --top-chrome-md=material (the current default).
        command_line->AppendSwitchASCII(switches::kTopChromeMD,
                                        switches::kTopChromeMDMaterial);
        break;
    }
  }

  views::Widget* CreatePopupForTestQuery();
  views::Widget* GetPopupWidget() { return popup_view()->GetWidget(); }
  OmniboxResultView* GetResultViewAt(int index) {
    return popup_view()->result_view_at(index);
  }

  ToolbarView* toolbar() {
    return BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  }
  LocationBarView* location_bar() { return toolbar()->location_bar(); }
  OmniboxViewViews* omnibox_view() { return location_bar()->omnibox_view(); }
  OmniboxEditModel* edit_model() { return omnibox_view()->model(); }
  OmniboxPopupModel* popup_model() { return edit_model()->popup_model(); }
  OmniboxPopupContentsView* popup_view() {
    return static_cast<OmniboxPopupContentsView*>(popup_model()->view());
  }

 private:
  test::ScopedMacViewsBrowserMode views_mode_{true};
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupContentsViewTest);
};

// Create an alias to instantiate tests that only care about the rounded style.
using RoundedOmniboxPopupContentsViewTest = OmniboxPopupContentsViewTest;

views::Widget* OmniboxPopupContentsViewTest::CreatePopupForTestQuery() {
  EXPECT_TRUE(popup_model()->result().empty());
  EXPECT_FALSE(popup_view()->IsOpen());
  EXPECT_FALSE(GetPopupWidget());

  edit_model()->SetUserText(base::ASCIIToUTF16("foo"));
  edit_model()->StartAutocomplete(false, false);

  EXPECT_FALSE(popup_model()->result().empty());
  EXPECT_TRUE(popup_view()->IsOpen());
  views::Widget* popup = GetPopupWidget();
  EXPECT_TRUE(popup);
  return popup;
}

// Tests widget alignment of the different popup types.
IN_PROC_BROWSER_TEST_P(OmniboxPopupContentsViewTest, PopupAlignment) {
  views::Widget* popup = CreatePopupForTestQuery();

  if (GetParam() == WIDE) {
    EXPECT_EQ(toolbar()->width(), popup->GetRestoredBounds().width());
  } else if (GetParam() == NARROW) {
    // Inset for the views::BubbleBorder::SMALL_SHADOW.
    gfx::Insets border_insets = popup_view()->GetInsets();
    EXPECT_EQ(location_bar()->width() + border_insets.width(),
              popup->GetRestoredBounds().width());
  } else {
    gfx::Rect alignment_rect = location_bar()->GetBoundsInScreen();
    alignment_rect.Inset(
        -RoundedOmniboxResultsFrame::kLocationBarAlignmentInsets);
    // Top, left and right should align. Bottom depends on the results.
    gfx::Rect popup_rect = popup->GetRestoredBounds();
    EXPECT_EQ(popup_rect.y(), alignment_rect.y());
    EXPECT_EQ(popup_rect.x(), alignment_rect.x());
    EXPECT_EQ(popup_rect.right(), alignment_rect.right());
  }
}

#if defined(OS_MACOSX)
// This test doesn't work on Mac because Mac doesn't theme the omnibox the way
// it expects.
#define MAYBE_ThemeIntegration DISABLED_ThemeIntegration
#else
#define MAYBE_ThemeIntegration ThemeIntegration
#endif

// Integration test for omnibox popup theming. This is a browser test since it
// relies on initialization done in chrome_browser_main_extra_parts_views_linux
// propagating through correctly to OmniboxPopupContentsView::GetTint().
IN_PROC_BROWSER_TEST_P(OmniboxPopupContentsViewTest, MAYBE_ThemeIntegration) {
  // Sanity check the bot: ensure the profile is configured to use the system
  // theme. On Linux, the default depends on a whitelist using the result of
  // base::nix::GetDesktopEnvironment(). E.g. KDE never uses the system theme.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  if (!theme_service->UsingSystemTheme()) {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseSystemTheme();
  }
  ASSERT_TRUE(theme_service->UsingSystemTheme());

  // Unthemed, non-incognito always has a white background. Exceptions: Inverted
  // color themes on Windows and GTK (not tested here).
  EXPECT_EQ(SK_ColorWHITE, GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND,
                                           popup_view()->GetTint()));

  Browser* browser_under_test = browser();

  // Helper to get the background selected color for |browser_under_test| using
  // omnibox_theme.
  auto get_selection_color = [&browser_under_test]() {
    LocationBarView* location_bar =
        BrowserView::GetBrowserViewForBrowser(browser_under_test)
            ->toolbar()
            ->location_bar();
    return GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND,
                           location_bar->tint(), OmniboxPartState::SELECTED);
  };

  // Helper to ensure consistency of colors obtained via the ui::NativeTheme
  // associated with |browser_under_test|. Technically, this should rely on the
  // popup's Widget (not the browser's), but the popup inherits this theme
  // whenever it is created. Correctness of this doesn't need to last forever:
  // we should avoid getting all colors via NativeTheme, since they are not all
  // native theme concepts (just sometimes derived from them).
  auto get_selection_color_from_widget = [&]() {
    return BrowserView::GetBrowserViewForBrowser(browser_under_test)
        ->GetNativeTheme()
        ->GetSystemColor(
            ui::NativeTheme::kColorId_ResultsTableSelectedBackground);
  };

  // Selection is derived from the native theme, except on the new style. This
  // gets a color from omnibox_theme with the style given by GetParam(), but it
  // is only used in checks when GetParam() is ROUNDED.
  const SkColor rounded_selection_color_light =
      GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND, OmniboxTint::LIGHT,
                      OmniboxPartState::SELECTED);
  const SkColor rounded_selection_color_dark =
      GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND, OmniboxTint::DARK,
                      OmniboxPartState::SELECTED);

  // Tests below are mainly interested just whether things change, so ensure
  // that can be detected.
  EXPECT_NE(rounded_selection_color_dark, rounded_selection_color_light);

  const SkColor default_selection_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_ResultsTableSelectedBackground);
  SkColor system_selection_color = default_selection_color;
#if defined(USE_X11)
  // On Linux, ensure GTK is fully plumbed through by getting expectations
  // directly from views::LinuxUI.
  ASSERT_TRUE(views::LinuxUI::instance());
  system_selection_color =
      views::LinuxUI::instance()->GetActiveSelectionBgColor();
#endif

  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_light, get_selection_color());
  } else {
    EXPECT_EQ(system_selection_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  SkColor legacy_dark_color = system_selection_color;

#if defined(OS_MACOSX) || defined(USE_X11)
  // Mac and system-themed Desktop Linux continue to use light theming in
  // incognito windows.
  const bool dark_used_in_system_incognito_theme = false;
#else
  const bool dark_used_in_system_incognito_theme = true;
#endif
#if defined(USE_AURA)
  legacy_dark_color = ui::NativeThemeDarkAura::instance()->GetSystemColor(
      ui::NativeTheme::kColorId_ResultsTableSelectedBackground);
#endif

  // Check unthemed incognito windows.
  Browser* incognito_browser = CreateIncognitoBrowser();
  browser_under_test = incognito_browser;
  if (GetParam() == ROUNDED) {
    EXPECT_EQ(dark_used_in_system_incognito_theme
                  ? rounded_selection_color_dark
                  : rounded_selection_color_light,
              get_selection_color());
  } else {
    EXPECT_EQ(dark_used_in_system_incognito_theme ? legacy_dark_color
                                                  : system_selection_color,
              get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  // Install a theme (in both browsers, since it's the same profile).
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  {
    ThemeChangeWaiter wait(theme_service);
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII("theme"));
    loader.LoadExtension(path);
  }

#if defined(OS_CHROMEOS)
  if (chromeos::GetAshConfig() == ash::Config::MASH) {
    // http://crbug.com/704942: Incognito frames do not update correctly in
    // Mash, so the old values remain. Check here so this is revisited when
    // fixed, and bail out of the rest of the test.
    if (GetParam() == ROUNDED)
      EXPECT_EQ(rounded_selection_color_dark, get_selection_color());
    else
      EXPECT_EQ(legacy_dark_color, get_selection_color());

    return;
  }
#endif

  // Check the incognito browser first. Everything should now be light and never
  // GTK.
  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_light, get_selection_color());
  } else {
    EXPECT_EQ(default_selection_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  // Same in the non-incognito browser .
  browser_under_test = browser();
  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_light, get_selection_color());
  } else {
    EXPECT_EQ(default_selection_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  // Undo the theme install. Check the regular browser goes back to native.
  {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseSystemTheme();
  }

  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_light, get_selection_color());
  } else {
    EXPECT_EQ(system_selection_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  // Switch to the default theme without installing a custom theme. E.g. this is
  // what gets used on KDE or when switching to the "classic" theme in settings.
  {
    ThemeChangeWaiter wait(theme_service);
    theme_service->UseDefaultTheme();
  }
  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_light, get_selection_color());
  } else {
    EXPECT_EQ(default_selection_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }

  // Check incognito again. It should now use a dark theme, even on Linux.
  browser_under_test = incognito_browser;
  if (GetParam() == ROUNDED) {
    EXPECT_EQ(rounded_selection_color_dark, get_selection_color());
  } else {
    EXPECT_EQ(legacy_dark_color, get_selection_color());
    EXPECT_EQ(get_selection_color_from_widget(), get_selection_color());
  }
}

// This is only enabled on ChromeOS for now, since it's hard to align an
// EventGenerator when multiple native windows are involved (the sanity check
// will fail). TODO(tapted): Enable everywhere.
#if defined(OS_CHROMEOS)
#define MAYBE_ClickOmnibox ClickOmnibox
#else
#define MAYBE_ClickOmnibox DISABLED_ClickOmnibox
#endif
// Test that clicks over the omnibox do not hit the popup.
IN_PROC_BROWSER_TEST_P(RoundedOmniboxPopupContentsViewTest,
                       MAYBE_ClickOmnibox) {
  CreatePopupForTestQuery();
  ui::test::EventGenerator generator(browser()->window()->GetNativeWindow());

  OmniboxResultView* result = GetResultViewAt(0);
  ASSERT_TRUE(result);

  // Sanity check: ensure the EventGenerator clicks where we think it should
  // when clicking on a result (but don't dismiss the popup yet). This will fail
  // if the WindowTreeHost and EventGenerator coordinate systems do not align.
  {
    const gfx::Point expected_point = result->GetLocalBounds().CenterPoint();
    EXPECT_NE(gfx::Point(), expected_point);

    gfx::Point click;
    ClickTrackingOverlayView overlay(result, &click);
    generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());
    generator.ClickLeftButton();
    EXPECT_EQ(expected_point, click);
  }

  // Select the text, so that we can test whether a click is received (which
  // should deselect the text);
  omnibox_view()->SelectAll(true);
  views::Textfield* textfield = omnibox_view();
  EXPECT_EQ(base::ASCIIToUTF16("foo"), textfield->GetSelectedText());

  generator.MoveMouseTo(location_bar()->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_EQ(base::string16(), textfield->GetSelectedText());

  // Clicking the result should dismiss the popup (asynchronously).
  generator.MoveMouseTo(result->GetBoundsInScreen().CenterPoint());

  ASSERT_TRUE(GetPopupWidget());
  EXPECT_FALSE(GetPopupWidget()->IsClosed());

  generator.ClickLeftButton();
  ASSERT_TRUE(GetPopupWidget());
  EXPECT_TRUE(GetPopupWidget()->IsClosed());
}

INSTANTIATE_TEST_CASE_P(,
                        OmniboxPopupContentsViewTest,
                        ::testing::Values(WIDE, NARROW, ROUNDED),
                        &PrintPopupType);

INSTANTIATE_TEST_CASE_P(,
                        RoundedOmniboxPopupContentsViewTest,
                        ::testing::Values(ROUNDED),
                        &PrintPopupType);
