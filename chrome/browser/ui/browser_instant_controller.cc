// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/sys_color_change_listener.h"

using content::UserMetricsAction;

namespace {

const char* GetInstantPrefName(Profile* profile) {
  return chrome::search::IsInstantExtendedAPIEnabled(profile) ?
      prefs::kInstantExtendedEnabled : prefs::kInstantEnabled;
}

}  // namespace

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
               chrome::search::IsInstantExtendedAPIEnabled(profile())),
      instant_unload_handler_(browser),
      initialized_theme_info_(false) {
  PrefService* prefs = profile()->GetPrefs();

  // The kInstantExtendedEnabled and kInstantEnabled preferences are
  // separate, as the way opt-in is done is a bit different, and
  // because the experiment that controls the behavior of
  // kInstantExtendedEnabled (value retrieved via
  // search::GetInstantExtendedDefaultSetting) may take different
  // settings on different Chrome set-ups for the same user.
  //
  // In one mode of the experiment, however, the
  // kInstantExtendedEnabled preference's default value is set to the
  // existing value of kInstantEnabled.
  //
  // Because this requires reading the value of the kInstantEnabled
  // value, we reset the default for kInstantExtendedEnabled here,
  // instead of fully determining the default in RegisterUserPrefs,
  // below.
  bool instant_extended_default = true;
  switch (search::GetInstantExtendedDefaultSetting()) {
    case search::INSTANT_DEFAULT_ON:
      instant_extended_default = true;
      break;
    case search::INSTANT_USE_EXISTING:
      instant_extended_default = prefs->GetBoolean(prefs::kInstantEnabled);
    case search::INSTANT_DEFAULT_OFF:
      instant_extended_default = false;
      break;
  }

  prefs->SetDefaultPrefValue(
      prefs::kInstantExtendedEnabled,
      Value::CreateBooleanValue(instant_extended_default));

  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      GetInstantPrefName(profile()),
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kSearchSuggestEnabled,
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  ResetInstant();
  browser_->search_model()->AddObserver(this);

#if defined(ENABLE_THEMES)
  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile())));
#endif  // defined(ENABLE_THEMES)
}

BrowserInstantController::~BrowserInstantController() {
  browser_->search_model()->RemoveObserver(this);
}

bool BrowserInstantController::IsInstantEnabled(Profile* profile) {
  return profile && !profile->IsOffTheRecord() && profile->GetPrefs() &&
         profile->GetPrefs()->GetBoolean(GetInstantPrefName(profile));
}

void BrowserInstantController::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kInstantEnabled, false,
                                PrefRegistrySyncable::SYNCABLE_PREF);

  // Note that the default for this pref gets reset in the
  // BrowserInstantController constructor.
  registry->RegisterBooleanPref(prefs::kInstantExtendedEnabled,
                                false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
}

bool BrowserInstantController::MaybeSwapInInstantNTPContents(
    const GURL& url,
    content::WebContents* source_contents,
    content::WebContents** target_contents) {
  if (url != GURL(chrome::kChromeUINewTabURL))
    return false;

  GURL extension_url(url);
  if (ExtensionWebUI::HandleChromeURLOverride(&extension_url, profile())) {
    // If there is an extension overriding the NTP do not use the Instant NTP.
    return false;
  }

  scoped_ptr<content::WebContents> instant_ntp = instant_.ReleaseNTPContents();
  if (!instant_ntp)
    return false;

  *target_contents = instant_ntp.get();
  if (source_contents) {
    instant_ntp->GetController().CopyStateFromAndPrune(
        &source_contents->GetController());
    ReplaceWebContentsAt(
        browser_->tab_strip_model()->GetIndexOfWebContents(source_contents),
        instant_ntp.Pass());
  } else {
    instant_ntp->GetController().PruneAllButActive();
    // If |source_contents| is NULL, then the caller is responsible for
    // inserting instant_ntp into the tabstrip and will take ownership.
    ignore_result(instant_ntp.release());
  }
  content::RecordAction(UserMetricsAction("InstantExtended.ShowNTP"));
  return true;
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

  return instant_.CommitIfPossible(disposition == CURRENT_TAB ?
      INSTANT_COMMIT_PRESSED_ENTER : INSTANT_COMMIT_PRESSED_ALT_ENTER);
}

Profile* BrowserInstantController::profile() const {
  return browser_->profile();
}

void BrowserInstantController::CommitInstant(
    scoped_ptr<content::WebContents> preview,
    bool in_new_tab) {
  if (profile()->GetExtensionService()->IsInstalledApp(preview->GetURL())) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_INSTANT);
  }
  if (in_new_tab) {
    // TabStripModel takes ownership of |preview|.
    browser_->tab_strip_model()->AddWebContents(preview.release(), -1,
        instant_.last_transition_type(), TabStripModel::ADD_ACTIVE);
  } else {
    ReplaceWebContentsAt(
        browser_->tab_strip_model()->active_index(),
        preview.Pass());
  }
}

void BrowserInstantController::ReplaceWebContentsAt(
    int index,
    scoped_ptr<content::WebContents> new_contents) {
  DCHECK_NE(TabStripModel::kNoTab, index);
  scoped_ptr<content::WebContents> old_contents(browser_->tab_strip_model()->
      ReplaceWebContentsAt(index, new_contents.release()));
  instant_unload_handler_.RunUnloadListenersOrDestroy(old_contents.Pass(),
                                                      index);
}

void BrowserInstantController::SetInstantSuggestion(
    const InstantSuggestion& suggestion) {
  browser_->window()->GetLocationBar()->SetInstantSuggestion(suggestion);
}

void BrowserInstantController::CommitSuggestedText(
    bool skip_inline_autocomplete) {
  browser_->window()->GetLocationBar()->GetLocationEntry()->model()->
      CommitSuggestedText(skip_inline_autocomplete);
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

void BrowserInstantController::TabDeactivated(content::WebContents* contents) {
  instant_.TabDeactivated(contents);
}

void BrowserInstantController::UpdateThemeInfo(bool parse_theme_info) {
  // Update theme background info.
  // Initialize or re-parse |theme_info| if necessary.
  if (!initialized_theme_info_ || parse_theme_info)
    OnThemeChanged(ThemeServiceFactory::GetForProfile(profile()));
  else
    OnThemeChanged(NULL);
}

void BrowserInstantController::OpenURL(
    const GURL& url,
    content::PageTransition transition,
    WindowOpenDisposition disposition) {
  browser_->OpenURL(content::OpenURLParams(url,
                                           content::Referrer(),
                                           disposition,
                                           transition,
                                           false));
}

void BrowserInstantController::SetOmniboxBounds(const gfx::Rect& bounds) {
  instant_.SetOmniboxBounds(bounds);
}

void BrowserInstantController::ResetInstant() {
  bool instant_enabled = IsInstantEnabled(profile());
  bool use_local_preview_only = profile()->IsOffTheRecord() ||
      (!instant_enabled &&
       !profile()->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled));
  instant_.SetInstantEnabled(instant_enabled, use_local_preview_only);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, search::SearchModelObserver implementation:

void BrowserInstantController::ModeChanged(const search::Mode& old_mode,
                                           const search::Mode& new_mode) {
  // If mode is now |NTP|, send theme-related information to instant.
  if (new_mode.is_ntp())
    UpdateThemeInfo(false);

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
        theme_service->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
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
      theme_service->GetDisplayProperty(
          ThemeProperties::NTP_BACKGROUND_ALIGNMENT, &alignment);
      if (alignment & ThemeProperties::ALIGN_LEFT) {
        theme_info_.image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
      } else if (alignment & ThemeProperties::ALIGN_RIGHT) {
        theme_info_.image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
      } else {  // ALIGN_CENTER
        theme_info_.image_horizontal_alignment =
            THEME_BKGRND_IMAGE_ALIGN_CENTER;
      }

      // Set theme background image vertical alignment.
      if (alignment & ThemeProperties::ALIGN_TOP) {
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
#if !defined(OS_ANDROID)
        // A detached bookmark bar will draw the top part of a top-aligned theme
        // image as its background, so offset the image by the bar height.
        if (browser_->bookmark_bar_state() == BookmarkBar::DETACHED)
          theme_info_.image_top_offset = -chrome::kNTPBookmarkBarHeight;
#endif  // !defined(OS_ANDROID)
      } else if (alignment & ThemeProperties::ALIGN_BOTTOM) {
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
      } else {  // ALIGN_CENTER
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;
      }

      // Set theme background image tiling.
      int tiling = 0;
      theme_service->GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING,
                                        &tiling);
      switch (tiling) {
        case ThemeProperties::NO_REPEAT:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
            break;
        case ThemeProperties::REPEAT_X:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
            break;
        case ThemeProperties::REPEAT_Y:
            theme_info_.image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
            break;
        case ThemeProperties::REPEAT:
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

  if (browser_->search_model()->mode().is_ntp())
    instant_.ThemeChanged(theme_info_);
}

}  // namespace chrome
