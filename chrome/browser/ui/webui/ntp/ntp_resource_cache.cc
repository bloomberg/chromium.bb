// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_constants.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/ntp_login_handler.h"
#include "chrome/browser/ui/webui/sync_setup_handler.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/sys_color_change_listener.h"

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
    "https://www.google.com/support/chromeos/bin/answer.py?answer=95464";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=95464";
#endif

// The URL for the Learn More page shown on guest session new tab.
const char kLearnMoreGuestSessionUrl[] =
    "https://www.google.com/support/chromeos/bin/answer.py?answer=1057090";

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

SkColor GetThemeColor(ui::ThemeProvider* tp, int id) {
  SkColor color = tp->GetColor(id);
  // If web contents are being inverted because the system is in high-contrast
  // mode, any system theme colors we use must be inverted too to cancel out.
  return gfx::IsInvertedColorScheme() ?
      color_utils::InvertColor(color) : color;
}

// Get the CSS string for the background position on the new tab page for the
// states when the bar is attached or detached.
std::string GetNewTabBackgroundCSS(const ui::ThemeProvider* theme_provider,
                                   bool bar_attached) {
  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/17593
  if (!theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  int alignment = theme_provider->GetDisplayProperty(
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
    const ui::ThemeProvider* theme_provider) {
  int repeat_mode = theme_provider->GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_TILING);
  return ThemeProperties::TilingToString(repeat_mode);
}

}  // namespace

NTPResourceCache::NTPResourceCache(Profile* profile)
    : profile_(profile), is_swipe_tracking_from_scroll_events_enabled_(false),
      should_show_apps_page_(NewTabUI::ShouldShowApps()),
      should_show_most_visited_page_(true),
      should_show_other_devices_menu_(true),
      should_show_recently_closed_menu_(true) {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile)));
  registrar_.Add(this, chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                 content::NotificationService::AllSources());

  base::Closure callback = base::Bind(&NTPResourceCache::OnPreferenceChanged,
                                      base::Unretained(this));

  // Watch for pref changes that cause us to need to invalidate the HTML cache.
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(prefs::kShowBookmarkBar, callback);
  profile_pref_change_registrar_.Add(prefs::kNtpShownPage, callback);
  profile_pref_change_registrar_.Add(prefs::kSignInPromoShowNTPBubble,
                                     callback);
  profile_pref_change_registrar_.Add(prefs::kHideWebStoreIcon, callback);

  // Some tests don't have a local state.
#if defined(ENABLE_APP_LIST)
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
    return NTPResourceCache::GUEST;
  } else if (render_host) {
    // Sometimes the |profile| is the parent (non-incognito) version of the user
    // so we check the |render_host| if it is provided.
    if (render_host->GetBrowserContext()->IsOffTheRecord())
      return NTPResourceCache::INCOGNITO;
  } else if (profile->IsOffTheRecord()) {
    return NTPResourceCache::INCOGNITO;
  }
  return NTPResourceCache::NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (win_type == GUEST) {
    if (!new_tab_guest_html_.get())
      CreateNewTabGuestHTML();
    return new_tab_guest_html_.get();
  } else if (win_type == INCOGNITO) {
    if (!new_tab_incognito_html_.get())
      CreateNewTabIncognitoHTML();
    return new_tab_incognito_html_.get();
  } else {
    // Refresh the cached HTML if necessary.
    // NOTE: NewTabCacheNeedsRefresh() must be called every time the new tab
    // HTML is fetched, because it needs to initialize cached values.
    if (NewTabCacheNeedsRefresh() || !new_tab_html_.get())
      CreateNewTabHTML();
    return new_tab_html_.get();
  }
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(WindowType win_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (win_type == GUEST) {
    if (!new_tab_guest_css_.get())
      CreateNewTabGuestCSS();
    return new_tab_guest_css_.get();
  } else if (win_type == INCOGNITO) {
    if (!new_tab_incognito_css_.get())
      CreateNewTabIncognitoCSS();
    return new_tab_incognito_css_.get();
  } else {
    if (!new_tab_css_.get())
      CreateNewTabCSS();
    return new_tab_css_.get();
  }
}

void NTPResourceCache::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  // Invalidate the cache.
  if (chrome::NOTIFICATION_BROWSER_THEME_CHANGED == type ||
      chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED == type) {
    new_tab_incognito_html_ = NULL;
    new_tab_html_ = NULL;
    new_tab_incognito_css_ = NULL;
    new_tab_css_ = NULL;
  } else {
    NOTREACHED();
  }
}

void NTPResourceCache::OnPreferenceChanged() {
  // A change occurred to one of the preferences we care about, so flush the
  // cache.
  new_tab_incognito_html_ = NULL;
  new_tab_html_ = NULL;
  new_tab_css_ = NULL;
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

  bool bookmark_bar_attached = profile_->GetPrefs()->GetBoolean(
      prefs::kShowBookmarkBar);
  localized_strings.SetBoolean("bookmarkbarattached", bookmark_bar_attached);

  webui::SetFontAndTextDirection(&localized_strings);

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
  guest_tab_link = kLearnMoreGuestSessionUrl;

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

  webui::SetFontAndTextDirection(&localized_strings);

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
      prefs->GetBoolean(prefs::kShowBookmarkBar));
  load_time_data.SetBoolean("hasattribution",
      ThemeServiceFactory::GetForProfile(profile_)->HasCustomImage(
          IDR_THEME_NTP_ATTRIBUTION));
  load_time_data.SetBoolean("showMostvisited", should_show_most_visited_page_);
  load_time_data.SetBoolean("showAppLauncherPromo",
      ShouldShowAppLauncherPromo());
  load_time_data.SetBoolean("showRecentlyClosed",
      should_show_recently_closed_menu_);
  load_time_data.SetString("title",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE));
  load_time_data.SetString("mostvisited",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_MOST_VISITED));
  load_time_data.SetString("suggestions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_SUGGESTIONS));
  load_time_data.SetString("restoreThumbnailsShort",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK));
  load_time_data.SetString("recentlyclosed",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED));
  load_time_data.SetString("webStoreTitle",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
  load_time_data.SetString("webStoreTitleShort",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE_SHORT));
  load_time_data.SetString("closedwindowsingle",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_SINGLE));
  load_time_data.SetString("closedwindowmultiple",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_RECENTLY_CLOSED_WINDOW_MULTIPLE));
  load_time_data.SetString("attributionintro",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_ATTRIBUTION_INTRO));
  load_time_data.SetString("thumbnailremovednotification",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION));
  load_time_data.SetString("undothumbnailremove",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE));
  load_time_data.SetString("removethumbnailtooltip",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP));
  load_time_data.SetString("appuninstall",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));
  load_time_data.SetString("appoptions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_OPTIONS));
  load_time_data.SetString("appdetails",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_DETAILS));
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
                            NTPLoginHandler::ShouldShow(profile_));
  load_time_data.SetString("otherSessions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_LABEL));
  load_time_data.SetString("otherSessionsEmpty",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_EMPTY));
  load_time_data.SetString("otherSessionsLearnMoreUrl",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_LEARN_MORE_URL));
  load_time_data.SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  load_time_data.SetString("webStoreLink",
      google_util::AppendGoogleLocaleParam(
          GURL(extension_urls::GetWebstoreLaunchURL()),
          g_browser_process->GetApplicationLocale()).spec());
  load_time_data.SetString("appInstallHintText",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_INSTALL_HINT_LABEL));
  load_time_data.SetBoolean("isDiscoveryInNTPEnabled",
      NewTabUI::IsDiscoveryInNTPEnabled());
  load_time_data.SetString("collapseSessionMenuItemText",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_COLLAPSE_SESSION));
  load_time_data.SetString("expandSessionMenuItemText",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_EXPAND_SESSION));
  load_time_data.SetString("restoreSessionMenuItemText",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_OTHER_SESSIONS_OPEN_ALL));
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
  // Supervised users can not have apps installed currently so there's no need
  // to show the app cards.
  if (profile_->IsSupervised())
    should_show_apps_page_ = false;

  load_time_data.SetBoolean("showApps", should_show_apps_page_);
  load_time_data.SetBoolean("showWebStoreIcon",
                            !prefs->GetBoolean(prefs::kHideWebStoreIcon));

  bool streamlined_hosted_apps = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableStreamlinedHostedApps);
  load_time_data.SetBoolean("enableStreamlinedHostedApps",
                            streamlined_hosted_apps);
  // Use a different string for launching as a regular tab for streamlined
  // hosted apps.
  if (streamlined_hosted_apps) {
    load_time_data.SetString("applaunchtypetab",
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_TAB));
  }

#if defined(OS_CHROMEOS)
  load_time_data.SetString("expandMenu",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_CLOSE_MENU_EXPAND));
#endif

  NewTabPageHandler::GetLocalizedValues(profile_, &load_time_data);
  NTPLoginHandler::GetLocalizedValues(profile_, &load_time_data);

  webui::SetFontAndTextDirection(&load_time_data);

  // Control fade and resize animations.
  load_time_data.SetBoolean("anim",
                            gfx::Animation::ShouldRenderRichAnimation());

  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  int alignment = tp->GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
  load_time_data.SetString("themegravity",
      (alignment & ThemeProperties::ALIGN_RIGHT) ? "right" : "");

  // Disable the promo if this is the first run, otherwise set the promo string
  // for display if there is a valid outstanding promo.
  if (first_run::IsChromeFirstRun()) {
    NotificationPromo::HandleClosed(NotificationPromo::NTP_NOTIFICATION_PROMO);
  } else {
    NotificationPromo notification_promo;
    notification_promo.InitFromPrefs(NotificationPromo::NTP_NOTIFICATION_PROMO);
    if (notification_promo.CanShow()) {
      load_time_data.SetString("notificationPromoText",
                               notification_promo.promo_text());
      DVLOG(1) << "Notification promo:" << notification_promo.promo_text();
    }

    NotificationPromo bubble_promo;
    bubble_promo.InitFromPrefs(NotificationPromo::NTP_BUBBLE_PROMO);
    if (bubble_promo.CanShow()) {
      load_time_data.SetString("bubblePromoText",
                               bubble_promo.promo_text());
      DVLOG(1) << "Bubble promo:" << bubble_promo.promo_text();
    }
  }

  // Determine whether to show the menu for accessing tabs on other devices.
  bool show_other_sessions_menu = should_show_other_devices_menu_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNTPOtherSessionsMenu);
  load_time_data.SetBoolean("showOtherSessionsMenu", show_other_sessions_menu);
  load_time_data.SetBoolean("isUserSignedIn",
      !prefs->GetString(prefs::kGoogleServicesUsername).empty());

  // Load the new tab page appropriate for this build.
  base::StringPiece new_tab_html(ResourceBundle::GetSharedInstance().
      GetRawDataResource(IDR_NEW_TAB_4_HTML));
  webui::UseVersion2 version2;
  std::string full_html =
      webui::GetI18nTemplateHtml(new_tab_html, &load_time_data);
  new_tab_html_ = base::RefCountedString::TakeString(&full_html);
}

void NTPResourceCache::CreateNewTabIncognitoCSS() {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND);

  // Generate the replacements.
  std::vector<std::string> subst;

  // Cache-buster for background.
  subst.push_back(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID));  // $1

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));  // $2
  subst.push_back(GetNewTabBackgroundCSS(tp, false));  // $3
  subst.push_back(GetNewTabBackgroundCSS(tp, true));  // $4
  subst.push_back(GetNewTabBackgroundTilingCSS(tp));  // $5

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_INCOGNITO_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string full_css = ReplaceStringPlaceholders(
      new_tab_theme_css, subst, NULL);

  new_tab_incognito_css_ = base::RefCountedString::TakeString(&full_css);
}

void NTPResourceCache::CreateNewTabGuestCSS() {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND);

  // Generate the replacements.
  std::vector<std::string> subst;

  // Cache-buster for background.
  subst.push_back(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID));  // $1

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));  // $2
  subst.push_back(GetNewTabBackgroundCSS(tp, false));  // $3
  subst.push_back(GetNewTabBackgroundCSS(tp, true));  // $4
  subst.push_back(GetNewTabBackgroundTilingCSS(tp));  // $5

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_GUEST_TAB_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string full_css = ReplaceStringPlaceholders(
      new_tab_theme_css, subst, NULL);

  new_tab_guest_css_ = base::RefCountedString::TakeString(&full_css);
}

void NTPResourceCache::CreateNewTabCSS() {
  ui::ThemeProvider* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  // Get our theme colors
  SkColor color_background =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor color_text = GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT);
  SkColor color_link = GetThemeColor(tp, ThemeProperties::COLOR_NTP_LINK);
  SkColor color_link_underline =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_LINK_UNDERLINE);

  SkColor color_section =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION);
  SkColor color_section_text =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_TEXT);
  SkColor color_section_link =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_LINK);
  SkColor color_section_link_underline =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_LINK_UNDERLINE);
  SkColor color_section_header_text =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_HEADER_TEXT);
  SkColor color_section_header_text_hover =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_HEADER_TEXT_HOVER);
  SkColor color_section_header_rule =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_HEADER_RULE);
  SkColor color_section_header_rule_light =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_SECTION_HEADER_RULE_LIGHT);
  SkColor color_text_light =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_TEXT_LIGHT);

  SkColor color_header =
      GetThemeColor(tp, ThemeProperties::COLOR_NTP_HEADER);
  // Generate a lighter color for the header gradients.
  color_utils::HSL header_lighter;
  color_utils::SkColorToHSL(color_header, &header_lighter);
  header_lighter.l += (1 - header_lighter.l) * 0.33;
  SkColor color_header_gradient_light =
      color_utils::HSLToSkColor(header_lighter, SkColorGetA(color_header));

  // Generate section border color from the header color. See
  // BookmarkBarView::Paint for how we do this for the bookmark bar
  // borders.
  SkColor color_section_border =
      SkColorSetARGB(80,
                     SkColorGetR(color_header),
                     SkColorGetG(color_header),
                     SkColorGetB(color_header));

  // Generate the replacements.
  std::vector<std::string> subst;

  // Cache-buster for background.
  subst.push_back(
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID));  // $1

  // Colors.
  subst.push_back(SkColorToRGBAString(color_background));  // $2
  subst.push_back(GetNewTabBackgroundCSS(tp, false));  // $3
  subst.push_back(GetNewTabBackgroundCSS(tp, true));  // $4
  subst.push_back(GetNewTabBackgroundTilingCSS(tp));  // $5
  subst.push_back(SkColorToRGBAString(color_header));  // $6
  subst.push_back(SkColorToRGBAString(color_header_gradient_light));  // $7
  subst.push_back(SkColorToRGBAString(color_text));  // $8
  subst.push_back(SkColorToRGBAString(color_link));  // $9
  subst.push_back(SkColorToRGBAString(color_section));  // $10
  subst.push_back(SkColorToRGBAString(color_section_border));  // $11
  subst.push_back(SkColorToRGBAString(color_section_text));  // $12
  subst.push_back(SkColorToRGBAString(color_section_link));  // $13
  subst.push_back(SkColorToRGBAString(color_link_underline));  // $14
  subst.push_back(SkColorToRGBAString(color_section_link_underline));  // $15
  subst.push_back(SkColorToRGBAString(color_section_header_text));  // $16
  subst.push_back(SkColorToRGBAString(
      color_section_header_text_hover));  // $17
  subst.push_back(SkColorToRGBAString(color_section_header_rule));  // $18
  subst.push_back(SkColorToRGBAString(
      color_section_header_rule_light));  // $19
  subst.push_back(SkColorToRGBAString(
      SkColorSetA(color_section_header_rule, 0)));  // $20
  subst.push_back(SkColorToRGBAString(color_text_light));  // $21
  subst.push_back(SkColorToRGBComponents(color_section_border));  // $22
  subst.push_back(SkColorToRGBComponents(color_text));  // $23

  // Get our template.
  static const base::StringPiece new_tab_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NEW_TAB_4_THEME_CSS));

  // Create the string from our template and the replacements.
  std::string css_string;
  css_string = ReplaceStringPlaceholders(new_tab_theme_css, subst, NULL);
  new_tab_css_ = base::RefCountedString::TakeString(&css_string);
}
