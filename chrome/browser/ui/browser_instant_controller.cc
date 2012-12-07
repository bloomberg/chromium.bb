// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/sys_color_change_listener.h"


namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
               chrome::search::IsInstantExtendedAPIEnabled(browser->profile())),
      instant_unload_handler_(browser),
      initialized_theme_info_(false),
      theme_area_height_(0) {
  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kInstantEnabled,
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  ResetInstant();
  browser_->search_model()->AddObserver(this);

#if defined(ENABLE_THEMES)
  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(browser_->profile())));
#endif  // defined(ENABLE_THEMES)
}

BrowserInstantController::~BrowserInstantController() {
  browser_->search_model()->RemoveObserver(this);
}

bool BrowserInstantController::IsInstantEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(prefs::kInstantEnabled);
}

void BrowserInstantController::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kInstantEnabled, false,
                             PrefService::SYNCABLE_PREF);
}

bool BrowserInstantController::OpenInstant(WindowOpenDisposition disposition) {
  // Unsupported dispositions.
  if (disposition == NEW_BACKGROUND_TAB || disposition == NEW_WINDOW)
    return false;

  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this DCHECK file a bug and I'll (sky) add
  // support for the new disposition.
  DCHECK(disposition == CURRENT_TAB ||
         disposition == NEW_FOREGROUND_TAB) << disposition;

  return instant_.CommitIfCurrent(disposition == CURRENT_TAB ?
      INSTANT_COMMIT_PRESSED_ENTER : INSTANT_COMMIT_PRESSED_ALT_ENTER);
}

void BrowserInstantController::CommitInstant(content::WebContents* preview,
                                             bool in_new_tab) {
  if (in_new_tab) {
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->AddWebContents(preview, -1,
        instant_.last_transition_type(), TabStripModel::ADD_ACTIVE);
  } else {
    int index = browser_->tab_strip_model()->active_index();
    DCHECK_NE(TabStripModel::kNoTab, index);
    content::WebContents* active_tab =
        browser_->tab_strip_model()->GetWebContentsAt(index);
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->ReplaceWebContentsAt(index, preview);
    // InstantUnloadHandler takes ownership of |active_tab|.
    instant_unload_handler_.RunUnloadListenersOrDestroy(active_tab, index);

    GURL url = preview->GetURL();
    DCHECK(browser_->profile()->GetExtensionService());
    if (browser_->profile()->GetExtensionService()->IsInstalledApp(url)) {
      AppLauncherHandler::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_OMNIBOX_INSTANT);
    }
  }
}

void BrowserInstantController::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  browser_->window()->GetLocationBar()->SetInstantSuggestion(suggestion);
}

gfx::Rect BrowserInstantController::GetInstantBounds() {
  return browser_->window()->GetInstantBounds();
}

void BrowserInstantController::InstantPreviewFocused() {
  // NOTE: This is only invoked on aura.
  browser_->window()->WebContentsFocused(instant_.GetPreviewContents());
}

void BrowserInstantController::FocusOmniboxInvisibly() {
  OmniboxView* omnibox_view = browser_->window()->GetLocationBar()->
      GetLocationEntry();
  omnibox_view->SetFocus();
  omnibox_view->model()->SetCaretVisibility(false);
}

content::WebContents* BrowserInstantController::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserInstantController::ActiveTabChanged() {
  instant_.ActiveTabChanged();
}

void BrowserInstantController::SetContentHeight(int height) {
  OnThemeAreaHeightChanged(height);
}

void BrowserInstantController::UpdateThemeInfoForPreview() {
  // Update theme background info and theme area height.
  // Initialize |theme_info| if necessary.
  // |OnThemeChanged| also updates theme area height if necessary.
  if (!initialized_theme_info_)
    OnThemeChanged(ThemeServiceFactory::GetForProfile(browser_->profile()));
  else
    OnThemeChanged(NULL);
}

void BrowserInstantController::ResetInstant() {
  instant_.SetInstantEnabled(IsInstantEnabled(browser_->profile()));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, search::SearchModelObserver implementation:

void BrowserInstantController::ModeChanged(const search::Mode& old_mode,
                                           const search::Mode& new_mode) {
  // If mode is now |NTP|, send theme-related information to instant.
  if (new_mode.is_ntp())
    UpdateThemeInfoForPreview();

  instant_.SearchModeChanged(old_mode, new_mode);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, content::NotificationObserver implementation:

void BrowserInstantController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(ENABLE_THEMES)
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  OnThemeChanged(content::Source<ThemeService>(source).ptr());
#endif  // defined(ENABLE_THEMES)
}

void BrowserInstantController::OnThemeChanged(ThemeService* theme_service) {
  if (theme_service) {  // Get theme information from theme service.
    theme_info_ = ThemeBackgroundInfo();

    // Set theme background color.
    SkColor background_color =
        theme_service->GetColor(ThemeService::COLOR_NTP_BACKGROUND);
    if (gfx::IsInvertedColorScheme())
      background_color = color_utils::InvertColor(background_color);
    theme_info_.color_r = SkColorGetR(background_color);
    theme_info_.color_g = SkColorGetG(background_color);
    theme_info_.color_b = SkColorGetB(background_color);
    theme_info_.color_a = SkColorGetA(background_color);

    if (theme_service->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
      // Set theme id for theme background image url.
      theme_info_.theme_id = theme_service->GetThemeID();

      // Set theme background image horizontal alignment.
      int alignment = 0;
      theme_service->GetDisplayProperty(ThemeService::NTP_BACKGROUND_ALIGNMENT,
                                        &alignment);
      if (alignment & ThemeService::ALIGN_LEFT) {
        theme_info_.image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
      } else if (alignment & ThemeService::ALIGN_RIGHT) {
        theme_info_.image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
      } else {  // ALIGN_CENTER
        theme_info_.image_horizontal_alignment =
            THEME_BKGRND_IMAGE_ALIGN_CENTER;
      }

      // Set theme background image vertical alignment.
      if (alignment & ThemeService::ALIGN_TOP)
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
      else if (alignment & ThemeService::ALIGN_BOTTOM)
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
      else // ALIGN_CENTER
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

      // Set theme background image tiling.
      int tiling = 0;
      theme_service->GetDisplayProperty(ThemeService::NTP_BACKGROUND_TILING,
                                        &tiling);
      switch (tiling) {
        case ThemeService::NO_REPEAT:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
            break;
        case ThemeService::REPEAT_X:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
            break;
        case ThemeService::REPEAT_Y:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
            break;
        case ThemeService::REPEAT:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_REPEAT;
            break;
      }

      // Set theme background image height.
      gfx::ImageSkia* image = theme_service->GetImageSkiaNamed(
          IDR_THEME_NTP_BACKGROUND);
      DCHECK(image);
      theme_info_.image_height = image->height();
    }

    initialized_theme_info_ = true;
  }

  DCHECK(initialized_theme_info_);

  if (browser_->search_model()->mode().is_ntp()) {
    instant_.ThemeChanged(theme_info_);

    // Theme area height is only sent to preview for non-top-aligned images;
    // new theme may have a different alignment that requires preview to know
    // theme area height.
    OnThemeAreaHeightChanged(theme_area_height_);
  }
}

void BrowserInstantController::OnThemeAreaHeightChanged(int height) {
  theme_area_height_ = height;

  // Notify preview only if mode is |NTP| and theme background image is not top-
  // aligned; top-aligned images don't need theme area height to determine which
  // part of the image overlay should draw, 'cos the origin is top-left.
  if (!browser_->search_model()->mode().is_ntp() ||
      theme_info_.theme_id.empty() ||
      theme_info_.image_vertical_alignment == THEME_BKGRND_IMAGE_ALIGN_TOP) {
    return;
  }
  instant_.ThemeAreaHeightChanged(theme_area_height_);
}

}  // namespace chrome
