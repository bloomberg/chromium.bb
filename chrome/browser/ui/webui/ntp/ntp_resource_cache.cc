// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/webui/app_launcher_login_handler.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/platform_util.h"
#endif

using content::BrowserThread;

namespace {

// The URL for the the Learn More page shown on incognito new tab.
const char kLearnMoreIncognitoUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/?p=incognito";
#else
    "https://support.google.com/chrome/?p=incognito";
#endif

// The URL for the Learn More page shown on guest session new tab.
const char kLearnMoreGuestSessionUrl[] =
#if defined(OS_CHROMEOS)
    "https://support.google.com/chromebook/answer/1057090";
#else
    "https://support.google.com/chrome/?p=ui_guest";
#endif

std::string SkColorToRGBAString(SkColor color) {
  // We convert the alpha using DoubleToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return base::StringPrintf(
      "rgba(%d,%d,%d,%s)",
      SkColorGetR(color),
      SkColorGetG(color),
      SkColorGetB(color),
      base::DoubleToString(SkColorGetA(color) / 255.0).c_str());
}

// Creates an rgb string for an SkColor, but leaves the alpha blank so that the
// css can fill it in.
std::string SkColorToRGBComponents(SkColor color) {
  return base::StringPrintf(
      "%d,%d,%d",
      SkColorGetR(color),
      SkColorGetG(color),
      SkColorGetB(color));
}

SkColor GetThemeColor(const ui::ThemeProvider& tp, int id) {
  SkColor color = tp.GetColor(id);
  // If web contents are being inverted because the system is in high-contrast
  // mode, any system theme colors we use must be inverted too to cancel out.
  return color_utils::IsInvertedColorScheme() ?
      color_utils::InvertColor(color) : color;
}

// Get the CSS string for the background position on the new tab page for the
// states when the bar is attached or detached.
std::string GetNewTabBackgroundCSS(const ui::ThemeProvider& theme_provider,
                                   bool bar_attached) {
  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  int alignment = theme_provider.GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT);

  if (bar_attached)
    return ThemeProperties::AlignmentToString(alignment);

  if (alignment & ThemeProperties::ALIGN_TOP) {
    // The bar is detached, so we must offset the background by the bar size
    // if it's a top-aligned bar.
    int offset = chrome::kNTPBookmarkBarHeight;

    if (alignment & ThemeProperties::ALIGN_LEFT)
      return "left " + base::IntToString(-offset) + "px";
    else if (alignment & ThemeProperties::ALIGN_RIGHT)
      return "right " + base::IntToString(-offset) + "px";
    return "center " + base::IntToString(-offset) + "px";
  }

  return ThemeProperties::AlignmentToString(alignment);
}

// How the background image on the new tab page should be tiled (see tiling
// masks in theme_service.h).
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider& theme_provider) {
  int repeat_mode =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING);
  return ThemeProperties::TilingToString(repeat_mode);
}

}  // namespace

NTPResourceCache::NTPResourceCache(Profile* profile)
    : profile_(profile), is_swipe_tracking_from_scroll_events_enabled_(false),
      should_show_apps_page_(NewTabUI::ShouldShowApps()),
      should_show_other_devices_menu_(true) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));

  base::Closure callback = base::Bind(&NTPResourceCache::OnPreferenceChanged,
                                      base::Unretained(this));

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(bookmarks::prefs::kShowBookmarkBar,
                                     callback);
  profile_pref_change_registrar_.Add(prefs::kNtpShownPage, callback);
  profile_pref_change_registrar_.Add(prefs::kSignInPromoShowNTPBubble,
                                     callback);
  profile_pref_change_registrar_.Add(prefs::kHideWebStoreIcon, callback);

  // Some tests don't have a local state.
#if BUILDFLAG(ENABLE_APP_LIST)
  if (g_browser_process->local_state()) {
    local_state_pref_change_registrar_.Init(g_browser_process->local_state());
    local_state_pref_change_registrar_.Add(prefs::kShowAppLauncherPromo,
                                           callback);
    local_state_pref_change_registrar_.Add(
        prefs::kAppLauncherHasBeenEnabled, callback);
  }
#endif
}

NTPResourceCache::~NTPResourceCache() {}

bool NTPResourceCache::NewTabCacheNeedsRefresh() {
#if defined(OS_MACOSX)
  // Invalidate if the current value is different from the cached value.
  bool is_enabled = platform_util::IsSwipeTrackingFromScrollEventsEnabled();
  if (is_enabled != is_swipe_tracking_from_scroll_events_enabled_) {
    is_swipe_tracking_from_scroll_events_enabled_ = is_enabled;
    return true;
  }
#endif
  bool should_show_apps_page = NewTabUI::ShouldShowApps();
  if (should_show_apps_page != should_show_apps_page_) {
    should_show_apps_page_ = should_show_apps_page;
    return true;
  }
  return false;
}

NTPResourceCache::WindowType NTPResourceCache::GetWindowType(
    Profile* profile, content::RenderProcessHost* render_host) {
  if (profile->IsGuestSession()) {
    return GUEST;
  } else if (render_host) {
    // Sometimes the |profile| is the parent (non-incognito) version of the user
    // so we check the |render_host| if it is provided.
    if (render_host->GetBrowserContext()->IsOffTheRecord())
      return INCOGNITO;
  } else if (profile->IsOffTheRecord()) {
    return INCOGNITO;
  }
  return NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (win_type == GUEST) {
    if (!new_tab_guest_html_)
      CreateNewTabGuestHTML();
    return new_tab_guest_html_.get();
  } else if (win_type == INCOGNITO) {
    if (!new_tab_incognito_html_)
      CreateNewTabIncognitoHTML();
    return new_tab_incognito_html_.get();
  } else {
    // Refresh the cached HTML if necessary.
    // NOTE: NewTabCacheNeedsRefresh() must be called every time the new tab
    // HTML is fetched, because it needs to initialize cached values.
    if (NewTabCacheNeedsRefresh() || !new_tab_html_)
      CreateNewTabHTML();
    return new_tab_html_.get();
  }
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Guest mode doesn't have theme-related CSS.
  if (win_type == GUEST)
    return nullptr;

  if (win_type == INCOGNITO) {
    if (!new_tab_incognito_css_)
      CreateNewTabIncognitoCSS();
    return new_tab_incognito_css_.get();
  }

  if (!new_tab_css_)
    CreateNewTabCSS();
  return new_tab_css_.get();
}

void NTPResourceCache::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);

  // Invalidate the cache.
  Invalidate();
}

void NTPResourceCache::OnPreferenceChanged() {
  // A change occurred to one of the preferences we care about, so flush the
  // cache.
  new_tab_incognito_html_ = nullptr;
  new_tab_html_ = nullptr;
  new_tab_css_ = nullptr;
}

// TODO(dbeam): why must Invalidate() and OnPreferenceChanged() both exist?
void NTPResourceCache::Invalidate() {
  new_tab_incognito_html_ = nullptr;
  new_tab_html_ = nullptr;
  new_tab_incognito_css_ = nullptr;
  new_tab_css_ = nullptr;
}

void NTPResourceCache::CreateNewTabIncognitoHTML() {
  base::DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  int new_tab_description_ids = IDS_NEW_TAB_OTR_DESCRIPTION;
  int new_tab_heading_ids = IDS_NEW_TAB_OTR_HEADING;
  int new_tab_link_ids = IDS_NEW_TAB_OTR_LEARN_MORE_LINK;
  int new_tab_warning_ids = IDS_NEW_TAB_OTR_MESSAGE_WARNING;
  int new_tab_html_idr = IDR_INCOGNITO_TAB_HTML;
  const char* new_tab_link = kLearnMoreIncognitoUrl;

  if (profile_->IsGuestSession()) {
    localized_strings.SetString("guestTabDescription",
        l10n_util::GetStringUTF16(new_tab_description_ids));
    localized_strings.SetString("guestTabHeading",
        l10n_util::GetStringUTF16(new_tab_heading_ids));
  } else {
    localized_strings.SetString("incognitoTabDescription",
        l10n_util::GetStringUTF16(new_tab_description_ids));
    localized_strings.SetString("incognitoTabHeading",
        l10n_util::GetStringUTF16(new_tab_heading_ids));
    localized_strings.SetString("incognitoTabWarning",
        l10n_util::GetStringUTF16(new_tab_warning_ids));
  }

  localized_strings.SetString("learnMore",
      l10n_util::GetStringUTF16(new_tab_link_ids));
  localized_strings.SetString("learnMoreLink", new_tab_link);

  bool bookmark_bar_attached =
      profile_->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
  localized_strings.SetBoolean("bookmarkbarattached", bookmark_bar_attached);

  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);
  localized_strings.SetBoolean("hasCustomBackground",
                               tp.HasCustomImage(IDR_THEME_NTP_BACKGROUND));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);

  static const base::StringPiece incognito_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          new_tab_html_idr));

  std::string full_html = webui::GetI18nTemplateHtml(
      incognito_tab_html, &localized_strings);

  new_tab_incognito_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabGuestHTML() {
  base::DictionaryValue localized_strings;
  localized_strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  const char* guest_tab_link = kLearnMoreGuestSessionUrl;
  int guest_tab_ids = IDR_GUEST_TAB_HTML;
  int guest_tab_description_ids = IDS_NEW_TAB_GUEST_SESSION_DESCRIPTION;
  int guest_tab_heading_ids = IDS_NEW_TAB_GUEST_SESSION_HEADING;
  int guest_tab_link_ids = IDS_NEW_TAB_GUEST_SESSION_LEARN_MORE_LINK;

#if defined(OS_CHROMEOS)
  guest_tab_ids = IDR_GUEST_SESSION_TAB_HTML;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  std::string enterprise_domain = connector->GetEnterpriseDomain();

  if (!enterprise_domain.empty()) {
    // Device is enterprise enrolled.
    localized_strings.SetString("enterpriseInfoVisible", "true");
    base::string16 enterprise_info = l10n_util::GetStringFUTF16(
        IDS_DEVICE_OWNED_BY_NOTICE,
        base::UTF8ToUTF16(enterprise_domain));
    localized_strings.SetString("enterpriseInfoMessage", enterprise_info);
    localized_strings.SetString("enterpriseLearnMore",
        l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    localized_strings.SetString("enterpriseInfoHintLink",
                                chrome::kLearnMoreEnterpriseURL);
  } else {
    localized_strings.SetString("enterpriseInfoVisible", "false");
  }
#endif

  localized_strings.SetString("guestTabDescription",
      l10n_util::GetStringUTF16(guest_tab_description_ids));
  localized_strings.SetString("guestTabHeading",
      l10n_util::GetStringUTF16(guest_tab_heading_ids));
  localized_strings.SetString("learnMore",
      l10n_util::GetStringUTF16(guest_tab_link_ids));
  localized_strings.SetString("learnMoreLink", guest_tab_link);

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &localized_strings);

  static const base::StringPiece guest_tab_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(guest_tab_ids));

  std::string full_html = webui::GetI18nTemplateHtml(
      guest_tab_html, &localized_strings);

  new_tab_guest_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabHTML() {
  // TODO(estade): these strings should be defined in their relevant handlers
  // (in GetLocalizedValues) and should have more legible names.
  // Show the profile name in the title and most visited labels if the current
  // profile is not the default.
  PrefService* prefs = profile_->GetPrefs();
  base::DictionaryValue load_time_data;
  load_time_data.SetBoolean("bookmarkbarattached",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  load_time_data.SetBoolean("showAppLauncherPromo",
      ShouldShowAppLauncherPromo());
  load_time_data.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  load_time_data.SetString("webStoreTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
  load_time_data.SetString("webStoreTitleShort",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE_SHORT));
  load_time_data.SetString("attributionintro",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  load_time_data.SetString("appuninstall",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
  load_time_data.SetString("appoptions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_OPTIONS));
  load_time_data.SetString("appdetails",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_DETAILS));
  load_time_data.SetString("appinfodialog",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_SHOW_INFO));
  load_time_data.SetString("appcreateshortcut",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_CREATE_SHORTCUT));
  load_time_data.SetString("appDefaultPageName",
      l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME));
  load_time_data.SetString("applaunchtypepinned",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_PINNED));
  load_time_data.SetString("applaunchtyperegular",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_REGULAR));
  load_time_data.SetString("applaunchtypewindow",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));
  load_time_data.SetString("applaunchtypefullscreen",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN));
  load_time_data.SetString("syncpromotext",
      l10n_util::GetStringUTF16(IDS_SYNC_START_SYNC_BUTTON_LABEL));
  load_time_data.SetString("syncLinkText",
      l10n_util::GetStringUTF16(IDS_SYNC_ADVANCED_OPTIONS));
  load_time_data.SetBoolean("shouldShowSyncLogin",
                            AppLauncherLoginHandler::ShouldShow(profile_));
  load_time_data.SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  load_time_data.SetString(
      "webStoreLink", google_util::AppendGoogleLocaleParam(
                          extension_urls::GetWebstoreLaunchURL(), app_locale)
                          .spec());
  load_time_data.SetString("appInstallHintText",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_INSTALL_HINT_LABEL));
  load_time_data.SetString("learn_more",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  load_time_data.SetString("tile_grid_screenreader_accessible_description",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TILE_GRID_ACCESSIBLE_DESCRIPTION));
  load_time_data.SetString("page_switcher_change_title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_PAGE_SWITCHER_CHANGE_TITLE));
  load_time_data.SetString("page_switcher_same_title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_PAGE_SWITCHER_SAME_TITLE));
  load_time_data.SetString("appsPromoTitle",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_PAGE_APPS_PROMO_TITLE));
  // On Mac OS X 10.7+, horizontal scrolling can be treated as a back or
  // forward gesture. Pass through a flag that indicates whether or not that
  // feature is enabled.
  load_time_data.SetBoolean("isSwipeTrackingFromScrollEventsEnabled",
                            is_swipe_tracking_from_scroll_events_enabled_);

  load_time_data.SetBoolean("showApps", should_show_apps_page_);
  load_time_data.SetBoolean("showWebStoreIcon",
                            !prefs->GetBoolean(prefs::kHideWebStoreIcon));

  load_time_data.SetBoolean("enableNewBookmarkApps",
                            extensions::util::IsNewBookmarkAppsEnabled());

  load_time_data.SetBoolean("canHostedAppsOpenInWindows",
                            extensions::util::CanHostedAppsOpenInWindows());

  load_time_data.SetBoolean("canShowAppInfoDialog",
                            CanShowAppInfoDialog());

  AppLauncherHandler::GetLocalizedValues(profile_, &load_time_data);
  AppLauncherLoginHandler::GetLocalizedValues(profile_, &load_time_data);

  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);

  // Control fade and resize animations.
  load_time_data.SetBoolean("anim",
                            gfx::Animation::ShouldRenderRichAnimation());

  load_time_data.SetBoolean(
      "isUserSignedIn",
      SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated());

  // Load the new tab page appropriate for this build.
  base::StringPiece new_tab_html(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_NEW_TAB_4_HTML));
  std::string full_html =
      webui::GetI18nTemplateHtml(new_tab_html, &load_time_data);
  new_tab_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabIncognitoCSS() {
  // TODO(estade): this returns a subtly incorrect theme provider because
  // |profile_| is actually not the incognito profile. See crbug.com/568388
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);

  // Get our theme colors
  SkColor color_background =
      tp.HasCustomImage(IDR_THEME_NTP_BACKGROUND)
          ? GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND)
          : SkColorSetRGB(0x32, 0x32, 0x32);

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] = SkColorToRGBAString(color_background);
  substitutions["backgroundBarDetached"] = GetNewTabBackgroundCSS(tp, false);
  substitutions["backgroundBarAttached"] = GetNewTabBackgroundCSS(tp, true);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_INCOGNITO_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string full_css =
      ui::ReplaceTemplateExpressions(new_tab_theme_css, substitutions);

  new_tab_incognito_css_ = base::RefCountedString::TakeString(&full_css);
}

void NTPResourceCache::CreateNewTabCSS() {
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);

  // Get our theme colors
  SkColor color_background =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor color_text = GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT);
  SkColor color_text_light =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT_LIGHT);

  SkColor color_header =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_HEADER);
  // Generate a lighter color for the header gradients.
  color_utils::HSL header_lighter;
  color_utils::SkColorToHSL(color_header, &header_lighter);
  header_lighter.l += (1 - header_lighter.l) * 0.33;

  // Generate section border color from the header color. See
  // BookmarkBarView::Paint for how we do this for the bookmark bar
  // borders.
  SkColor color_section_border =
      SkColorSetARGB(80,
                     SkColorGetR(color_header),
                     SkColorGetG(color_header),
                     SkColorGetB(color_header));

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] = SkColorToRGBAString(color_background);
  substitutions["backgroundBarDetached"] = GetNewTabBackgroundCSS(tp, false);
  substitutions["backgroundBarAttached"] = GetNewTabBackgroundCSS(tp, true);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);
  substitutions["colorTextRgba"] = SkColorToRGBAString(color_text);
  substitutions["colorTextLight"] = SkColorToRGBAString(color_text_light);
  substitutions["colorSectionBorder"] =
      SkColorToRGBComponents(color_section_border);
  substitutions["colorText"] = SkColorToRGBComponents(color_text);

  // For themes that right-align the background, we flip the attribution to the
  // left to avoid conflicts.
  int alignment =
      tp.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
  if (alignment & ThemeProperties::ALIGN_RIGHT) {
    substitutions["leftAlignAttribution"] = "0";
    substitutions["rightAlignAttribution"] = "auto";
    substitutions["textAlignAttribution"] = "right";
  } else {
    substitutions["leftAlignAttribution"] = "auto";
    substitutions["rightAlignAttribution"] = "0";
    substitutions["textAlignAttribution"] = "left";
  }

  substitutions["displayAttribution"] =
      tp.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ? "inline" : "none";

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_4_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string css_string =
      ui::ReplaceTemplateExpressions(new_tab_theme_css, substitutions);
  new_tab_css_ = base::RefCountedString::TakeString(&css_string);
}
