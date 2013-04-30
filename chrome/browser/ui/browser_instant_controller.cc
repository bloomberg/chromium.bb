// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_instant_controller.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/sys_color_change_listener.h"

using content::UserMetricsAction;

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, public:

BrowserInstantController::BrowserInstantController(Browser* browser)
    : browser_(browser),
      instant_(this,
               chrome::IsInstantExtendedAPIEnabled()),
      instant_unload_handler_(browser),
      initialized_theme_info_(false) {

  // In one mode of the InstantExtended experiments, the kInstantExtendedEnabled
  // preference's default value is set to the existing value of kInstantEnabled.
  // Because this requires reading the value of the kInstantEnabled value, we
  // reset the default for kInstantExtendedEnabled here.
  chrome::SetInstantExtendedPrefDefault(profile());

  profile_pref_registrar_.Init(profile()->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kInstantEnabled,
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kInstantExtendedEnabled,
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kSearchSuggestEnabled,
      base::Bind(&BrowserInstantController::ResetInstant,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kDefaultSearchProviderID,
      base::Bind(&BrowserInstantController::OnDefaultSearchProviderChanged,
                 base::Unretained(this)));
  ResetInstant(std::string());
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
    scoped_ptr<content::WebContents> overlay,
    bool in_new_tab) {
  const extensions::Extension* extension =
      profile()->GetExtensionService()->GetInstalledApp(overlay->GetURL());
  if (extension) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_INSTANT,
        extension->GetType());
  }
  if (in_new_tab) {
    // TabStripModel takes ownership of |overlay|.
    browser_->tab_strip_model()->AddWebContents(overlay.release(), -1,
        instant_.last_transition_type(), TabStripModel::ADD_ACTIVE);
  } else {
    content::WebContents* contents = overlay.get();
    ReplaceWebContentsAt(
        browser_->tab_strip_model()->active_index(),
        overlay.Pass());
    browser_->window()->GetLocationBar()->SaveStateToContents(contents);
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

void BrowserInstantController::InstantOverlayFocused() {
  // NOTE: This is only invoked on aura.
  browser_->window()->WebContentsFocused(instant_.GetOverlayContents());
}

void BrowserInstantController::FocusOmnibox(bool caret_visibility) {
  OmniboxView* omnibox_view = browser_->window()->GetLocationBar()->
      GetLocationEntry();
  omnibox_view->SetFocus();
  omnibox_view->model()->SetCaretVisibility(caret_visibility);
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

void BrowserInstantController::UpdateThemeInfo() {
  // Update theme background info.
  // Initialize |theme_info| if necessary.
  if (!initialized_theme_info_)
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

void BrowserInstantController::ResetInstant(const std::string& pref_name) {
  // Update the default value of the kInstantExtendedEnabled pref to match the
  // value of the kInstantEnabled pref, if necessary.
  if (pref_name == prefs::kInstantEnabled)
    chrome::SetInstantExtendedPrefDefault(profile());

  bool instant_checkbox_checked = chrome::IsInstantCheckboxChecked(profile());
  bool use_local_overlay_only =
      chrome::IsLocalOnlyInstantExtendedAPIEnabled() ||
      !chrome::IsInstantCheckboxEnabled(profile());
  instant_.SetInstantEnabled(instant_checkbox_checked, use_local_overlay_only);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserInstantController, SearchModelObserver implementation:

void BrowserInstantController::ModelChanged(
    const SearchModel::State& old_state,
    const SearchModel::State& new_state) {
  if (old_state.mode == new_state.mode)
    return;

  const SearchMode& new_mode = new_state.mode;

  if (chrome::IsInstantExtendedAPIEnabled()) {
    // Record some actions corresponding to the mode change. Note that to get
    // the full story, it's necessary to look at other UMA actions as well,
    // such as tab switches.
    if (new_mode.is_search_results())
      content::RecordAction(UserMetricsAction("InstantExtended.ShowSRP"));
    else if (new_mode.is_ntp())
      content::RecordAction(UserMetricsAction("InstantExtended.ShowNTP"));
  }

  // If mode is now |NTP|, send theme-related information to Instant.
  if (new_mode.is_ntp())
    UpdateThemeInfo();

  instant_.SearchModeChanged(old_state.mode, new_mode);
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
      if (alignment & ThemeProperties::ALIGN_TOP)
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
      else if (alignment & ThemeProperties::ALIGN_BOTTOM)
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
      else  // ALIGN_CENTER
        theme_info_.image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

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

      theme_info_.has_attribution =
          theme_service->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
    }

    initialized_theme_info_ = true;
  }

  DCHECK(initialized_theme_info_);

  if (browser_->search_model()->mode().is_ntp())
    instant_.ThemeChanged(theme_info_);
}

void BrowserInstantController::OnDefaultSearchProviderChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kDefaultSearchProviderID));

  Profile* browser_profile = profile();
  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(browser_profile)->
          GetDefaultSearchProvider();
  if (!template_url) {
    // A NULL |template_url| could mean either this notification is sent during
    // the browser start up operation or the user now has no default search
    // provider. There is no way for the user to reach this state using the
    // Chrome settings. Only explicitly poking at the DB or bugs in the Sync
    // could cause that, neither of which we support.
    return;
  }

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_profile);
  if (!instant_service)
    return;

  TabStripModel* tab_model = browser_->tab_strip_model();
  int count = tab_model->count();
  for (int index = 0; index < count; ++index) {
    content::WebContents* contents = tab_model->GetWebContentsAt(index);
    if (!contents)
      continue;

    // A Local NTP always runs in the Instant process, so reloading it is
    // neither useful nor necessary. However, the Local NTP does not reflect
    // whether Google is the default search engine or not. This is achieved
    // through a URL parameter, so reloading the existing URL won't fix that
    // (i.e., the Local NTP may now show an incorrect search engine logo).
    // TODO(kmadhusu): Fix.
    if (contents->GetURL() == GURL(chrome::kChromeSearchLocalNtpUrl))
      continue;

    if (!instant_service->IsInstantProcess(
            contents->GetRenderProcessHost()->GetID()))
      continue;

    // Reload the contents to ensure that it gets assigned to a non-priviledged
    // renderer.
    contents->GetController().Reload(false);
  }
  instant_.OnDefaultSearchProviderChanged();
}
