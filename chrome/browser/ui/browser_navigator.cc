// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_tab_contents.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using content::GlobalRequestID;
using content::WebContents;

class BrowserNavigatorWebContentsAdoption {
 public:
  static void AttachTabHelpers(content::WebContents* contents) {
    BrowserTabContents::AttachTabHelpers(contents);
  }
};

namespace {

// Returns true if the specified Browser can open tabs. Not all Browsers support
// multiple tabs, such as app frames and popups. This function returns false for
// those types of Browser.
bool WindowCanOpenTabs(Browser* browser) {
  return browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP) ||
      browser->tab_strip_model()->empty();
}

// Finds an existing Browser compatible with |profile|, making a new one if no
// such Browser is located.
Browser* GetOrCreateBrowser(Profile* profile,
                            chrome::HostDesktopType host_desktop_type) {
  Browser* browser = chrome::FindTabbedBrowser(profile, false,
                                               host_desktop_type);
  return browser ? browser : new Browser(
      Browser::CreateParams(profile, host_desktop_type));
}

// Change some of the navigation parameters based on the particular URL.
// Currently this applies to some chrome:// pages which we always want to open
// in a non-incognito window. Note that even though a ChromeOS guest session is
// technically an incognito window, these URLs are allowed.
// Returns true on success. Otherwise, if changing params leads the browser into
// an erroneous state, returns false.
bool AdjustNavigateParamsForURL(chrome::NavigateParams* params) {
  if (params->target_contents != NULL ||
      chrome::IsURLAllowedInIncognito(params->url,
                                      params->initiating_profile) ||
      params->initiating_profile->IsGuestSession()) {
    return true;
  }

  Profile* profile = params->initiating_profile;

  if (profile->IsOffTheRecord() || params->disposition == OFF_THE_RECORD) {
    profile = profile->GetOriginalProfile();

    // If incognito is forced, we punt.
    PrefService* prefs = profile->GetPrefs();
    if (prefs && IncognitoModePrefs::GetAvailability(prefs) ==
            IncognitoModePrefs::FORCED) {
      return false;
    }

    params->disposition = SINGLETON_TAB;
    params->browser =
        chrome::FindOrCreateTabbedBrowser(profile, params->host_desktop_type);
    params->window_action = chrome::NavigateParams::SHOW_WINDOW;
  }

  return true;
}

// Returns a Browser that can host the navigation or tab addition specified in
// |params|. This might just return the same Browser specified in |params|, or
// some other if that Browser is deemed incompatible.
Browser* GetBrowserForDisposition(chrome::NavigateParams* params) {
  // If no source WebContents was specified, we use the selected one from
  // the target browser. This must happen first, before
  // GetBrowserForDisposition() has a chance to replace |params->browser| with
  // another one.
  if (!params->source_contents && params->browser) {
    params->source_contents =
        params->browser->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile = params->initiating_profile;

  switch (params->disposition) {
    case CURRENT_TAB:
      if (params->browser)
        return params->browser;
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return GetOrCreateBrowser(profile, params->host_desktop_type);
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
      // See if we can open the tab in the window this navigator is bound to.
      if (params->browser && WindowCanOpenTabs(params->browser))
        return params->browser;
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return GetOrCreateBrowser(profile, params->host_desktop_type);
    case NEW_POPUP: {
      // Make a new popup window.
      // Coerce app-style if |source| represents an app.
      std::string app_name;
      if (!params->extension_app_id.empty()) {
        app_name = web_app::GenerateApplicationNameFromExtensionId(
            params->extension_app_id);
      } else if (!params->browser->app_name().empty()) {
        app_name = params->browser->app_name();
      } else if (params->source_contents) {
        extensions::TabHelper* extensions_tab_helper =
            extensions::TabHelper::FromWebContents(params->source_contents);
        if (extensions_tab_helper && extensions_tab_helper->is_app()) {
          app_name = web_app::GenerateApplicationNameFromExtensionId(
              extensions_tab_helper->extension_app()->id());
        }
      }
      if (app_name.empty()) {
        Browser::CreateParams browser_params(
            Browser::TYPE_POPUP, profile, params->host_desktop_type);
        browser_params.initial_bounds = params->window_bounds;
        return new Browser(browser_params);
      }

      return new Browser(Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, app_name, params->window_bounds, profile,
          params->host_desktop_type));
    }
    case NEW_WINDOW: {
      // Make a new normal browser window.
      return new Browser(Browser::CreateParams(profile,
                                               params->host_desktop_type));
    }
    case OFF_THE_RECORD:
      // Make or find an incognito window.
      return GetOrCreateBrowser(profile->GetOffTheRecordProfile(),
                                params->host_desktop_type);
    // The following types all result in no navigation.
    case SUPPRESS_OPEN:
    case SAVE_TO_DISK:
    case IGNORE_ACTION:
      return NULL;
    default:
      NOTREACHED();
  }
  return NULL;
}

// Fix disposition and other parameter values depending on prevailing
// conditions.
void NormalizeDisposition(chrome::NavigateParams* params) {
  // Calculate the WindowOpenDisposition if necessary.
  if (params->browser->tab_strip_model()->empty() &&
      (params->disposition == NEW_BACKGROUND_TAB ||
       params->disposition == CURRENT_TAB ||
       params->disposition == SINGLETON_TAB)) {
    params->disposition = NEW_FOREGROUND_TAB;
  }
  if (params->browser->profile()->IsOffTheRecord() &&
      params->disposition == OFF_THE_RECORD) {
    params->disposition = NEW_FOREGROUND_TAB;
  }
  if (!params->source_contents && params->disposition == CURRENT_TAB)
    params->disposition = NEW_FOREGROUND_TAB;

  switch (params->disposition) {
    case NEW_BACKGROUND_TAB:
      // Disposition trumps add types. ADD_ACTIVE is a default, so we need to
      // remove it if disposition implies the tab is going to open in the
      // background.
      params->tabstrip_add_types &= ~TabStripModel::ADD_ACTIVE;
      break;

    case NEW_WINDOW:
    case NEW_POPUP:
      // Code that wants to open a new window typically expects it to be shown
      // automatically.
      if (params->window_action == chrome::NavigateParams::NO_ACTION)
        params->window_action = chrome::NavigateParams::SHOW_WINDOW;
      // Fall-through.
    case NEW_FOREGROUND_TAB:
    case SINGLETON_TAB:
      params->tabstrip_add_types |= TabStripModel::ADD_ACTIVE;
      break;

    default:
      break;
  }
}

// Obtain the profile used by the code that originated the Navigate() request.
Profile* GetSourceProfile(chrome::NavigateParams* params) {
  if (params->source_contents) {
    return Profile::FromBrowserContext(
        params->source_contents->GetBrowserContext());
  }

  return params->initiating_profile;
}

void LoadURLInContents(WebContents* target_contents,
                       const GURL& url,
                       chrome::NavigateParams* params) {
  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = params->referrer;
  load_url_params.transition_type = params->transition;
  load_url_params.extra_headers = params->extra_headers;
  load_url_params.is_cross_site_redirect = params->is_cross_site_redirect;

  if (params->transferred_global_request_id != GlobalRequestID()) {
    load_url_params.is_renderer_initiated = params->is_renderer_initiated;
    load_url_params.transferred_global_request_id =
        params->transferred_global_request_id;
  } else if (params->is_renderer_initiated) {
    load_url_params.is_renderer_initiated = true;
  }
  target_contents->GetController().LoadURLWithParams(load_url_params);
}

// This class makes sure the Browser object held in |params| is made visible
// by the time it goes out of scope, provided |params| wants it to be shown.
class ScopedBrowserDisplayer {
 public:
  explicit ScopedBrowserDisplayer(chrome::NavigateParams* params)
      : params_(params) {
  }
  ~ScopedBrowserDisplayer() {
    if (params_->window_action == chrome::NavigateParams::SHOW_WINDOW_INACTIVE)
      params_->browser->window()->ShowInactive();
    else if (params_->window_action == chrome::NavigateParams::SHOW_WINDOW)
      params_->browser->window()->Show();
  }
 private:
  chrome::NavigateParams* params_;
  DISALLOW_COPY_AND_ASSIGN(ScopedBrowserDisplayer);
};

// This class manages the lifetime of a WebContents created by the
// Navigate() function. When Navigate() creates a WebContents for a URL,
// an instance of this class takes ownership of it via TakeOwnership() until the
// WebContents is added to a tab strip at which time ownership is
// relinquished via ReleaseOwnership(). If this object goes out of scope without
// being added to a tab strip, the created WebContents is deleted to
// avoid a leak and the params->target_contents field is set to NULL.
class ScopedTargetContentsOwner {
 public:
  explicit ScopedTargetContentsOwner(chrome::NavigateParams* params)
      : params_(params) {
  }
  ~ScopedTargetContentsOwner() {
    if (target_contents_owner_.get())
      params_->target_contents = NULL;
  }

  // Assumes ownership of |params_|' target_contents until ReleaseOwnership
  // is called.
  void TakeOwnership() {
    target_contents_owner_.reset(params_->target_contents);
  }

  // Relinquishes ownership of |params_|' target_contents.
  WebContents* ReleaseOwnership() {
    return target_contents_owner_.release();
  }

 private:
  chrome::NavigateParams* params_;
  scoped_ptr<WebContents> target_contents_owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTargetContentsOwner);
};

content::WebContents* CreateTargetContents(const chrome::NavigateParams& params,
                                           const GURL& url) {
  WebContents::CreateParams create_params(
      params.browser->profile(),
      tab_util::GetSiteInstanceForNewTab(params.browser->profile(), url));
  if (params.source_contents) {
    create_params.initial_size =
        params.source_contents->GetView()->GetContainerSize();
  }
#if defined(USE_AURA)
  if (params.browser->window() &&
      params.browser->window()->GetNativeWindow()) {
    create_params.context =
        params.browser->window()->GetNativeWindow();
  }
#endif

  content::WebContents* target_contents = WebContents::Create(create_params);
  // New tabs can have WebUI URLs that will make calls back to arbitrary
  // tab helpers, so the entire set of tab helpers needs to be set up
  // immediately.
  BrowserNavigatorWebContentsAdoption::AttachTabHelpers(target_contents);
  extensions::TabHelper::FromWebContents(target_contents)->
      SetExtensionAppById(params.extension_app_id);
  // TODO(sky): Figure out why this is needed. Without it we seem to get
  // failures in startup tests.
  // By default, content believes it is not hidden.  When adding contents
  // in the background, tell it that it's hidden.
  if ((params.tabstrip_add_types & TabStripModel::ADD_ACTIVE) == 0) {
    // TabStripModel::AddWebContents invokes WasHidden if not foreground.
    target_contents->WasHidden();
  }
  return target_contents;
}

// If a prerendered page exists for |url|, replace the page at |target_contents|
// with it.
bool SwapInPrerender(WebContents* target_contents, const GURL& url) {
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          Profile::FromBrowserContext(target_contents->GetBrowserContext()));
  return prerender_manager &&
      prerender_manager->MaybeUsePrerenderedPage(target_contents, url);
}

bool SwapInInstantNTP(chrome::NavigateParams* params,
                      const GURL& url,
                      content::WebContents* source_contents) {
  BrowserInstantController* instant = params->browser->instant_controller();
  return instant && instant->MaybeSwapInInstantNTPContents(
      url, source_contents, &params->target_contents);
}

}  // namespace

namespace chrome {

NavigateParams::NavigateParams(Browser* a_browser,
                               const GURL& a_url,
                               content::PageTransition a_transition)
    : url(a_url),
      target_contents(NULL),
      source_contents(NULL),
      disposition(CURRENT_TAB),
      transition(a_transition),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(NO_ACTION),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
      browser(a_browser),
      initiating_profile(NULL),
      is_cross_site_redirect(false) {
        if (a_browser)
          host_desktop_type = a_browser->host_desktop_type();
        else
          host_desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
      }

NavigateParams::NavigateParams(Browser* a_browser,
                               WebContents* a_target_contents)
    : target_contents(a_target_contents),
      source_contents(NULL),
      disposition(CURRENT_TAB),
      transition(content::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(NO_ACTION),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
      browser(a_browser),
      initiating_profile(NULL),
      is_cross_site_redirect(false) {
        if (a_browser)
          host_desktop_type = a_browser->host_desktop_type();
        else
          host_desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
      }

NavigateParams::NavigateParams(Profile* a_profile,
                               const GURL& a_url,
                               content::PageTransition a_transition)
    : url(a_url),
      target_contents(NULL),
      source_contents(NULL),
      disposition(NEW_FOREGROUND_TAB),
      transition(a_transition),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(SHOW_WINDOW),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
      browser(NULL),
      initiating_profile(a_profile),
      host_desktop_type(chrome::HOST_DESKTOP_TYPE_NATIVE),
      is_cross_site_redirect(false) {}

NavigateParams::~NavigateParams() {}

void FillNavigateParamsFromOpenURLParams(chrome::NavigateParams* nav_params,
                                         const content::OpenURLParams& params) {
  nav_params->referrer = params.referrer;
  nav_params->extra_headers = params.extra_headers;
  nav_params->disposition = params.disposition;
  nav_params->override_encoding = params.override_encoding;
  nav_params->is_renderer_initiated = params.is_renderer_initiated;
  nav_params->transferred_global_request_id =
      params.transferred_global_request_id;
  nav_params->is_cross_site_redirect = params.is_cross_site_redirect;
}

void Navigate(NavigateParams* params) {
  Browser* source_browser = params->browser;
  if (source_browser)
    params->initiating_profile = source_browser->profile();
  DCHECK(params->initiating_profile);

  if (!AdjustNavigateParamsForURL(params))
    return;

  ExtensionService* service = params->initiating_profile->GetExtensionService();
  if (service)
    service->ShouldBlockUrlInBrowserTab(&params->url);

  // The browser window may want to adjust the disposition.
  if (params->disposition == NEW_POPUP &&
      source_browser &&
      source_browser->window()) {
    params->disposition =
        source_browser->window()->GetDispositionForPopupBounds(
            params->window_bounds);
  }

  params->browser = GetBrowserForDisposition(params);
  if (!params->browser)
    return;

  // Navigate() must not return early after this point.

  if (GetSourceProfile(params) != params->browser->profile()) {
    // A tab is being opened from a link from a different profile, we must reset
    // source information that may cause state to be shared.
    params->source_contents = NULL;
    params->referrer = content::Referrer();
  }

  // Make sure the Browser is shown if params call for it.
  ScopedBrowserDisplayer displayer(params);

  // Makes sure any WebContents created by this function is destroyed if
  // not properly added to a tab strip.
  ScopedTargetContentsOwner target_contents_owner(params);

  // Some dispositions need coercion to base types.
  NormalizeDisposition(params);

  // If a new window has been created, it needs to be displayed.
  if (params->window_action == NavigateParams::NO_ACTION &&
      source_browser != params->browser &&
      params->browser->tab_strip_model()->empty()) {
    params->window_action = NavigateParams::SHOW_WINDOW;
  }

  // If we create a popup window from a non user-gesture, don't activate it.
  if (params->window_action == NavigateParams::SHOW_WINDOW &&
      params->disposition == NEW_POPUP &&
      params->user_gesture == false) {
    params->window_action = NavigateParams::SHOW_WINDOW_INACTIVE;
  }

  // Determine if the navigation was user initiated. If it was, we need to
  // inform the target WebContents, and we may need to update the UI.
  content::PageTransition base_transition =
      content::PageTransitionStripQualifier(params->transition);
  bool user_initiated =
      params->transition & content::PAGE_TRANSITION_FROM_ADDRESS_BAR ||
      base_transition == content::PAGE_TRANSITION_TYPED ||
      base_transition == content::PAGE_TRANSITION_AUTO_BOOKMARK ||
      base_transition == content::PAGE_TRANSITION_GENERATED ||
      base_transition == content::PAGE_TRANSITION_AUTO_TOPLEVEL ||
      base_transition == content::PAGE_TRANSITION_RELOAD ||
      base_transition == content::PAGE_TRANSITION_KEYWORD;

  // Check if this is a singleton tab that already exists
  int singleton_index = chrome::GetIndexOfSingletonTab(params);

  // Did we use Instant's NTP contents?
  bool swapped_in_instant = false;

  // If no target WebContents was specified, we need to construct one if
  // we are supposed to target a new tab; unless it's a singleton that already
  // exists.
  if (!params->target_contents && singleton_index < 0) {
    GURL url;
    if (params->url.is_empty()) {
      url = params->browser->profile()->GetHomePage();
      params->transition = content::PageTransitionFromInt(
          params->transition | content::PAGE_TRANSITION_HOME_PAGE);
    } else {
      url = params->url;
    }

    if (params->disposition != CURRENT_TAB) {
      swapped_in_instant = SwapInInstantNTP(params, url, NULL);
      if (!swapped_in_instant)
        params->target_contents = CreateTargetContents(*params, url);

      // This function takes ownership of |params->target_contents| until it
      // is added to a TabStripModel.
      target_contents_owner.TakeOwnership();
    } else {
      // ... otherwise if we're loading in the current tab, the target is the
      // same as the source.
      DCHECK(params->source_contents);
      swapped_in_instant = SwapInInstantNTP(params, url,
                                            params->source_contents);
      if (!swapped_in_instant)
        params->target_contents = params->source_contents;
      DCHECK(params->target_contents);
    }

    if (user_initiated)
      params->target_contents->UserGestureDone();

    if (!swapped_in_instant) {
      if (SwapInPrerender(params->target_contents, url))
        return;

      // Try to handle non-navigational URLs that popup dialogs and such, these
      // should not actually navigate.
      if (!HandleNonNavigationAboutURL(url)) {
        // Perform the actual navigation, tracking whether it came from the
        // renderer.

        LoadURLInContents(params->target_contents, url, params);
      }
    }
  } else {
    // |target_contents| was specified non-NULL, and so we assume it has already
    // been navigated appropriately. We need to do nothing more other than
    // add it to the appropriate tabstrip.
  }

  // If the user navigated from the omnibox, and the selected tab is going to
  // lose focus, then make sure the focus for the source tab goes away from the
  // omnibox.
  if (params->source_contents &&
      (params->disposition == NEW_FOREGROUND_TAB ||
       params->disposition == NEW_WINDOW) &&
      (params->tabstrip_add_types & TabStripModel::ADD_INHERIT_OPENER))
    params->source_contents->GetView()->Focus();

  if (params->source_contents == params->target_contents ||
      (swapped_in_instant && params->disposition == CURRENT_TAB)) {
    // The navigation occurred in the source tab.
    params->browser->UpdateUIForNavigationInTab(params->target_contents,
                                                params->transition,
                                                user_initiated);
  } else if (singleton_index == -1) {
    // If some non-default value is set for the index, we should tell the
    // TabStripModel to respect it.
    if (params->tabstrip_index != -1)
      params->tabstrip_add_types |= TabStripModel::ADD_FORCE_INDEX;

    // The navigation should insert a new tab into the target Browser.
    params->browser->tab_strip_model()->AddWebContents(
        params->target_contents,
        params->tabstrip_index,
        params->transition,
        params->tabstrip_add_types);
    // Now that the |params->target_contents| is safely owned by the target
    // Browser's TabStripModel, we can release ownership.
    target_contents_owner.ReleaseOwnership();
  }

  if (singleton_index >= 0) {
    WebContents* target =
        params->browser->tab_strip_model()->GetWebContentsAt(singleton_index);

    if (target->IsCrashed()) {
      target->GetController().Reload(true);
    } else if (params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE &&
        target->GetURL() != params->url) {
      LoadURLInContents(target, params->url, params);
    }

    // If the singleton tab isn't already selected, select it.
    if (params->source_contents != params->target_contents) {
      params->browser->tab_strip_model()->ActivateTabAt(singleton_index,
                                                        user_initiated);
    }
  }

  if (params->disposition != CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_ADDED,
        content::Source<content::WebContentsDelegate>(params->browser),
        content::Details<WebContents>(params->target_contents));
  }
}

bool IsURLAllowedInIncognito(const GURL& url,
                             content::BrowserContext* browser_context) {
  // Most URLs are allowed in incognito; the following are exceptions.
  // chrome://extensions is on the list because it redirects to
  // chrome://settings.
  if (url.scheme() == chrome::kChromeUIScheme &&
      (url.host() == chrome::kChromeUISettingsHost ||
       url.host() == chrome::kChromeUISettingsFrameHost ||
       url.host() == chrome::kChromeUIExtensionsHost ||
       url.host() == chrome::kChromeUIBookmarksHost ||
       url.host() == chrome::kChromeUISyncPromoHost ||
       url.host() == chrome::kChromeUIUberHost)) {
    return false;
  }

  GURL rewritten_url = url;
  bool reverse_on_redirect = false;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url, browser_context, &reverse_on_redirect);

  // Some URLs are mapped to uber subpages. Do not allow them in incognito.
  return !(rewritten_url.scheme() == chrome::kChromeUIScheme &&
           rewritten_url.host() == chrome::kChromeUIUberHost);
}

}  // namespace chrome
