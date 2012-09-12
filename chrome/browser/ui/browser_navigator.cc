// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
#include "net/http/http_util.h"

using content::GlobalRequestID;
using content::WebContents;

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
Browser* GetOrCreateBrowser(Profile* profile) {
  Browser* browser = browser::FindTabbedBrowser(profile, false);
  return browser ? browser : new Browser(Browser::CreateParams(profile));
}

// Change some of the navigation parameters based on the particular URL.
// Currently this applies to some chrome:// pages which we always want to open
// in a non-incognito window. Note that even though a ChromeOS guest session is
// technically an incognito window, these URLs are allowed.
// Returns true on success. Otherwise, if changing params leads the browser into
// an erroneous state, returns false.
bool AdjustNavigateParamsForURL(chrome::NavigateParams* params) {
  if (params->target_contents != NULL ||
      chrome::IsURLAllowedInIncognito(params->url) ||
      Profile::IsGuestSession()) {
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
    params->browser = browser::FindOrCreateTabbedBrowser(profile);
    params->window_action = chrome::NavigateParams::SHOW_WINDOW;
  }

  return true;
}

// Returns a Browser that can host the navigation or tab addition specified in
// |params|. This might just return the same Browser specified in |params|, or
// some other if that Browser is deemed incompatible.
Browser* GetBrowserForDisposition(chrome::NavigateParams* params) {
  // If no source TabContents was specified, we use the selected one from
  // the target browser. This must happen first, before
  // GetBrowserForDisposition() has a chance to replace |params->browser| with
  // another one.
  if (!params->source_contents && params->browser)
    params->source_contents = chrome::GetActiveTabContents(params->browser);

  Profile* profile = params->initiating_profile;

  switch (params->disposition) {
    case CURRENT_TAB:
      if (params->browser)
        return params->browser;
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return GetOrCreateBrowser(profile);
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
      // See if we can open the tab in the window this navigator is bound to.
      if (params->browser && WindowCanOpenTabs(params->browser))
        return params->browser;
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      return GetOrCreateBrowser(profile);
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
            extensions::TabHelper::FromWebContents(
                params->source_contents->web_contents());
        if (extensions_tab_helper->is_app()) {
          app_name = web_app::GenerateApplicationNameFromExtensionId(
              extensions_tab_helper->extension_app()->id());
        }
      }
      if (app_name.empty()) {
        Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile);
        browser_params.initial_bounds = params->window_bounds;
        return new Browser(browser_params);
      }

      return new Browser(Browser::CreateParams::CreateForApp(
          Browser::TYPE_POPUP, app_name, params->window_bounds, profile));
    }
    case NEW_WINDOW: {
      // Make a new normal browser window.
      return new Browser(Browser::CreateParams(profile));
    }
    case OFF_THE_RECORD:
      // Make or find an incognito window.
      return GetOrCreateBrowser(profile->GetOffTheRecordProfile());
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
  if (params->source_contents)
    return params->source_contents->profile();

  return params->initiating_profile;
}

void LoadURLInContents(WebContents* target_contents,
                       const GURL& url,
                       chrome::NavigateParams* params,
                       const std::string& extra_headers) {
  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = params->referrer;
  load_url_params.transition_type = params->transition;
  load_url_params.extra_headers = extra_headers;

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

// This class manages the lifetime of a TabContents created by the
// Navigate() function. When Navigate() creates a TabContents for a URL,
// an instance of this class takes ownership of it via TakeOwnership() until the
// TabContents is added to a tab strip at which time ownership is
// relinquished via ReleaseOwnership(). If this object goes out of scope without
// being added to a tab strip, the created TabContents is deleted to
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
  TabContents* ReleaseOwnership() {
    return target_contents_owner_.release();
  }

 private:
  chrome::NavigateParams* params_;
  scoped_ptr<TabContents> target_contents_owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTargetContentsOwner);
};

void InitializeExtraHeaders(chrome::NavigateParams* params,
                            Profile* profile,
                            std::string* extra_headers) {
#if defined(ENABLE_RLZ)
  // If this is a home page navigation, check to see if the home page is
  // set to Google and add RLZ HTTP headers to the request.  This is only
  // done if Google was the original home page, and not changed afterwards by
  // the user.
  if (profile &&
      (params->transition & content::PAGE_TRANSITION_HOME_PAGE) != 0) {
    PrefService* pref_service = profile->GetPrefs();
    if (pref_service) {
      if (!pref_service->GetBoolean(prefs::kHomePageChanged)) {
        std::string homepage = pref_service->GetString(prefs::kHomePage);
        if (google_util::IsGoogleHomePageUrl(homepage)) {
          string16 rlz_string;
          RLZTracker::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, &rlz_string);
          if (!rlz_string.empty()) {
            net::HttpUtil::AppendHeaderIfMissing("X-Rlz-String",
                                                 UTF16ToUTF8(rlz_string),
                                                 extra_headers);
          }
        }
      }
    }
  }
#endif
}

// If a prerendered page exists for |url|, replace the page at |target_contents|
// with it.
bool SwapInPrerender(TabContents* target_contents, const GURL& url) {
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          target_contents->profile());
  WebContents* web_contents = target_contents->web_contents();
  return prerender_manager &&
      prerender_manager->MaybeUsePrerenderedPage(web_contents, url);
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
      initiating_profile(NULL) {}

NavigateParams::NavigateParams(Browser* a_browser,
                               TabContents* a_target_contents)
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
      initiating_profile(NULL) {}

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
      initiating_profile(a_profile) {}

NavigateParams::~NavigateParams() {}

void Navigate(NavigateParams* params) {
  Browser* source_browser = params->browser;
  if (source_browser)
    params->initiating_profile = source_browser->profile();
  DCHECK(params->initiating_profile);

  if (!AdjustNavigateParamsForURL(params))
    return;

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

  // Makes sure any TabContents created by this function is destroyed if
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
  // inform the target TabContents, and we may need to update the UI.
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

  std::string extra_headers;

  // Check if this is a singleton tab that already exists
  int singleton_index = chrome::GetIndexOfSingletonTab(params);

  // If no target TabContents was specified, we need to construct one if
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
      WebContents* source_contents = params->source_contents ?
          params->source_contents->web_contents() : NULL;
      params->target_contents =
          chrome::TabContentsFactory(
              params->browser->profile(),
              tab_util::GetSiteInstanceForNewTab(
                  params->browser->profile(), url),
              MSG_ROUTING_NONE,
              source_contents);
      // This function takes ownership of |params->target_contents| until it
      // is added to a TabStripModel.
      target_contents_owner.TakeOwnership();
      extensions::TabHelper::FromWebContents(
          params->target_contents->web_contents())->
              SetExtensionAppById(params->extension_app_id);
      // TODO(sky): figure out why this is needed. Without it we seem to get
      // failures in startup tests.
      // By default, content believes it is not hidden.  When adding contents
      // in the background, tell it that it's hidden.
      if ((params->tabstrip_add_types & TabStripModel::ADD_ACTIVE) == 0) {
        // TabStripModel::AddTabContents invokes WasHidden if not foreground.
        params->target_contents->web_contents()->WasHidden();
      }
    } else {
      // ... otherwise if we're loading in the current tab, the target is the
      // same as the source.
      params->target_contents = params->source_contents;
      DCHECK(params->target_contents);
    }

    if (user_initiated)
      params->target_contents->web_contents()->UserGestureDone();

    InitializeExtraHeaders(params, params->target_contents->profile(),
                           &extra_headers);

    if (SwapInPrerender(params->target_contents, url))
      return;

    // Try to handle non-navigational URLs that popup dialogs and such, these
    // should not actually navigate.
    if (!HandleNonNavigationAboutURL(url)) {
      // Perform the actual navigation, tracking whether it came from the
      // renderer.

      LoadURLInContents(params->target_contents->web_contents(),
                        url, params, extra_headers);
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
    params->source_contents->web_contents()->Focus();

  if (params->source_contents == params->target_contents) {
    // The navigation occurred in the source tab.
    params->browser->UpdateUIForNavigationInTab(
        params->target_contents,
        params->transition,
        user_initiated);
  } else if (singleton_index == -1) {
    // If some non-default value is set for the index, we should tell the
    // TabStripModel to respect it.
    if (params->tabstrip_index != -1)
      params->tabstrip_add_types |= TabStripModel::ADD_FORCE_INDEX;

    // The navigation should insert a new tab into the target Browser.
    params->browser->tab_strip_model()->AddTabContents(
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
        chrome::GetWebContentsAt(params->browser, singleton_index);

    if (target->IsCrashed()) {
      target->GetController().Reload(true);
    } else if (params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE &&
        target->GetURL() != params->url) {
      TabContents* target_tab = TabContents::FromWebContents(target);
      InitializeExtraHeaders(params, target_tab->profile(), &extra_headers);
      LoadURLInContents(target, params->url, params, extra_headers);
    }

    // If the singleton tab isn't already selected, select it.
    if (params->source_contents != params->target_contents)
      chrome::ActivateTabAt(params->browser, singleton_index, user_initiated);
  }

  if (params->disposition != CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_ADDED,
        content::Source<content::WebContentsDelegate>(params->browser),
        content::Details<WebContents>(params->target_contents->web_contents()));
  }
}

bool IsURLAllowedInIncognito(const GURL& url) {
  // Most URLs are allowed in incognito; the following are exceptions.
  // chrome://extensions is on the list because it redirects to
  // chrome://settings.

  return !(url.scheme() == chrome::kChromeUIScheme &&
      (url.host() == chrome::kChromeUISettingsHost ||
       url.host() == chrome::kChromeUISettingsFrameHost ||
       url.host() == chrome::kChromeUIExtensionsHost ||
       url.host() == chrome::kChromeUIBookmarksHost ||
       url.host() == chrome::kChromeUISyncPromoHost ||
       url.host() == chrome::kChromeUIUberHost));
}

}  // namespace chrome
