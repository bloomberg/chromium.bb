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
#include "chrome/browser/extensions/extension_tab_helper.h"
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
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
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
  return browser ? browser : Browser::Create(profile);
}

// Returns true if two URLs are equal after taking |replacements| into account.
bool CompareURLsWithReplacements(
    const GURL& url,
    const GURL& other,
    const url_canon::Replacements<char>& replacements) {
  if (url == other)
    return true;

  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL other_replaced = other.ReplaceComponents(replacements);
  return url_replaced == other_replaced;
}

// Change some of the navigation parameters based on the particular URL.
// Currently this applies to some chrome:// pages which we always want to open
// in a non-incognito window. Note that even though a ChromeOS guest session is
// technically an incognito window, these URLs are allowed.
// Returns true on success. Otherwise, if changing params leads the browser into
// an erroneous state, returns false.
bool AdjustNavigateParamsForURL(browser::NavigateParams* params) {
  if (params->target_contents != NULL ||
      browser::IsURLAllowedInIncognito(params->url) ||
      Profile::IsGuestSession()) {
    return true;
  }

  Profile* profile =
      params->browser ? params->browser->profile() : params->profile;

  if (profile->IsOffTheRecord() || params->disposition == OFF_THE_RECORD) {
    profile = profile->GetOriginalProfile();

    // If incognito is forced, we punt.
    PrefService* prefs = profile->GetPrefs();
    if (prefs && IncognitoModePrefs::GetAvailability(prefs) ==
            IncognitoModePrefs::FORCED) {
      return false;
    }

    params->disposition = SINGLETON_TAB;
    params->profile = profile;
    params->browser = browser::FindOrCreateTabbedBrowser(profile);
    params->window_action = browser::NavigateParams::SHOW_WINDOW;
  }

  return true;
}

// Returns a Browser that can host the navigation or tab addition specified in
// |params|. This might just return the same Browser specified in |params|, or
// some other if that Browser is deemed incompatible.
Browser* GetBrowserForDisposition(browser::NavigateParams* params) {
  // If no source TabContentsWrapper was specified, we use the selected one from
  // the target browser. This must happen first, before
  // GetBrowserForDisposition() has a chance to replace |params->browser| with
  // another one.
  if (!params->source_contents && params->browser)
    params->source_contents =
        params->browser->GetSelectedTabContentsWrapper();

  Profile* profile =
      params->browser ? params->browser->profile() : params->profile;

  switch (params->disposition) {
    case CURRENT_TAB:
      if (!params->browser && profile) {
        // We specified a profile instead of a browser; find or create one.
        params->browser = browser::FindOrCreateTabbedBrowser(profile);
      }
      return params->browser;
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
      // See if we can open the tab in the window this navigator is bound to.
      if (params->browser && WindowCanOpenTabs(params->browser))
        return params->browser;
      // Find a compatible window and re-execute this command in it. Otherwise
      // re-run with NEW_WINDOW.
      if (profile)
        return GetOrCreateBrowser(profile);
      return NULL;
    case NEW_POPUP: {
      // Make a new popup window.
      if (profile) {
        // Coerce app-style if |params->browser| or |source| represents an app.
        std::string app_name;
        if (!params->extension_app_id.empty()) {
          app_name = web_app::GenerateApplicationNameFromExtensionId(
              params->extension_app_id);
        } else if (params->browser && !params->browser->app_name().empty()) {
          app_name = params->browser->app_name();
        } else if (params->source_contents &&
                   params->source_contents->extension_tab_helper()->is_app()) {
          app_name = web_app::GenerateApplicationNameFromExtensionId(
              params->source_contents->extension_tab_helper()->
                  extension_app()->id());
        }
        if (app_name.empty()) {
          Browser::CreateParams browser_params(Browser::TYPE_POPUP, profile);
          browser_params.initial_bounds = params->window_bounds;
          return Browser::CreateWithParams(browser_params);
        } else {
          return Browser::CreateWithParams(
              Browser::CreateParams::CreateForApp(
                  Browser::TYPE_POPUP, app_name, params->window_bounds,
                  profile));
        }
      }
      return NULL;
    }
    case NEW_WINDOW:
      // Make a new normal browser window.
      if (profile) {
        Browser* browser = new Browser(Browser::TYPE_TABBED, profile);
        browser->InitBrowserWindow();
        return browser;
      }
      return NULL;
    case OFF_THE_RECORD:
      // Make or find an incognito window.
      if (profile)
        return GetOrCreateBrowser(profile->GetOffTheRecordProfile());
      return NULL;
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
void NormalizeDisposition(browser::NavigateParams* params) {
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
      if (params->window_action == browser::NavigateParams::NO_ACTION)
        params->window_action = browser::NavigateParams::SHOW_WINDOW;
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
// |source_browser| represents the Browser that was supplied in |params| before
// it was modified.
Profile* GetSourceProfile(browser::NavigateParams* params,
    Browser* source_browser) {
  if (params->source_contents)
    return params->source_contents->profile();

  if (source_browser)
    return source_browser->profile();

  if (params->profile)
    return params->profile;

  // We couldn't find one in any of the source metadata, so we'll fall back to
  // the profile associated with the target browser.
  return params->browser->profile();
}

void LoadURLInContents(WebContents* target_contents,
                       const GURL& url,
                       browser::NavigateParams* params,
                       const std::string& extra_headers) {
  if (params->transferred_global_request_id != GlobalRequestID()) {
    target_contents->GetController().TransferURL(
        url,
        params->referrer,
        params->transition, extra_headers,
        params->transferred_global_request_id,
        params->is_renderer_initiated);
  } else if (params->is_renderer_initiated) {
    target_contents->GetController().LoadURLFromRenderer(
        url,
        params->referrer,
        params->transition,  extra_headers);
  } else {
    target_contents->GetController().LoadURL(
        url,
        params->referrer,
        params->transition,  extra_headers);
  }

}

// This class makes sure the Browser object held in |params| is made visible
// by the time it goes out of scope, provided |params| wants it to be shown.
class ScopedBrowserDisplayer {
 public:
  explicit ScopedBrowserDisplayer(browser::NavigateParams* params)
      : params_(params) {
  }
  ~ScopedBrowserDisplayer() {
    if (params_->window_action == browser::NavigateParams::SHOW_WINDOW_INACTIVE)
      params_->browser->window()->ShowInactive();
    else if (params_->window_action == browser::NavigateParams::SHOW_WINDOW)
      params_->browser->window()->Show();
  }
 private:
  browser::NavigateParams* params_;
  DISALLOW_COPY_AND_ASSIGN(ScopedBrowserDisplayer);
};

// This class manages the lifetime of a TabContentsWrapper created by the
// Navigate() function. When Navigate() creates a TabContentsWrapper for a URL,
// an instance of this class takes ownership of it via TakeOwnership() until the
// TabContentsWrapper is added to a tab strip at which time ownership is
// relinquished via ReleaseOwnership(). If this object goes out of scope without
// being added to a tab strip, the created TabContentsWrapper is deleted to
// avoid a leak and the params->target_contents field is set to NULL.
class ScopedTargetContentsOwner {
 public:
  explicit ScopedTargetContentsOwner(browser::NavigateParams* params)
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
  TabContentsWrapper* ReleaseOwnership() {
    return target_contents_owner_.release();
  }

 private:
  browser::NavigateParams* params_;
  scoped_ptr<TabContentsWrapper> target_contents_owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTargetContentsOwner);
};

void InitializeExtraHeaders(browser::NavigateParams* params,
                            Profile* profile,
                            std::string* extra_headers) {
#if defined(ENABLE_RLZ)
  if (!profile)
    profile = params->profile;

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
bool SwapInPrerender(TabContentsWrapper* target_contents, const GURL& url) {
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(
          target_contents->profile());
  WebContents* web_contents = target_contents->web_contents();
  return prerender_manager &&
      prerender_manager->MaybeUsePrerenderedPage(web_contents, url);
}

}  // namespace

namespace browser {

NavigateParams::NavigateParams(
    Browser* a_browser,
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
      profile(NULL) {
}

NavigateParams::NavigateParams(Browser* a_browser,
                               TabContentsWrapper* a_target_contents)
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
      profile(NULL) {
}

NavigateParams::~NavigateParams() {
}

void Navigate(NavigateParams* params) {
  Browser* source_browser = params->browser;

  if (!AdjustNavigateParamsForURL(params))
    return;

  // The browser window may want to adjust the disposition.
  if (params->disposition == NEW_POPUP &&
      (source_browser && source_browser->window())) {
    params->disposition =
        source_browser->window()->GetDispositionForPopupBounds(
            params->window_bounds);
  }

  // Adjust disposition for the navigation happending in the sad page of the
  // panel window.
  if (params->source_contents &&
      params->source_contents->web_contents()->IsCrashed() &&
      params->disposition == CURRENT_TAB &&
      params->browser->is_type_panel()) {
    params->disposition = NEW_FOREGROUND_TAB;
  }

  params->browser = GetBrowserForDisposition(params);

  if (!params->browser)
    return;

  // Navigate() must not return early after this point.

  if (GetSourceProfile(params, source_browser) != params->browser->profile()) {
    // A tab is being opened from a link from a different profile, we must reset
    // source information that may cause state to be shared.
    params->source_contents = NULL;
    params->referrer = content::Referrer();
  }

  // Make sure the Browser is shown if params call for it.
  ScopedBrowserDisplayer displayer(params);

  // Makes sure any TabContentsWrapper created by this function is destroyed if
  // not properly added to a tab strip.
  ScopedTargetContentsOwner target_contents_owner(params);

  // Some dispositions need coercion to base types.
  NormalizeDisposition(params);

  // If a new window has been created, it needs to be displayed.
  if (params->window_action == browser::NavigateParams::NO_ACTION &&
      source_browser != params->browser &&
      params->browser->tab_strip_model()->empty()) {
    params->window_action = browser::NavigateParams::SHOW_WINDOW;
  }

  // If we create a popup window from a non user-gesture, don't activate it.
  if (params->window_action == browser::NavigateParams::SHOW_WINDOW &&
      params->disposition == NEW_POPUP &&
      params->user_gesture == false) {
    params->window_action = browser::NavigateParams::SHOW_WINDOW_INACTIVE;
  }

  // Determine if the navigation was user initiated. If it was, we need to
  // inform the target TabContentsWrapper, and we may need to update the UI.
  content::PageTransition base_transition =
      content::PageTransitionStripQualifier(params->transition);
  bool user_initiated =
      params->transition & content::PAGE_TRANSITION_FROM_ADDRESS_BAR ||
      base_transition == content::PAGE_TRANSITION_TYPED ||
      base_transition == content::PAGE_TRANSITION_AUTO_BOOKMARK ||
      base_transition == content::PAGE_TRANSITION_GENERATED ||
      base_transition == content::PAGE_TRANSITION_START_PAGE ||
      base_transition == content::PAGE_TRANSITION_RELOAD ||
      base_transition == content::PAGE_TRANSITION_KEYWORD;

  std::string extra_headers;

  // Check if this is a singleton tab that already exists
  int singleton_index = GetIndexOfSingletonTab(params);

  // If no target TabContentsWrapper was specified, we need to construct one if
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
          Browser::TabContentsFactory(
              params->browser->profile(),
              tab_util::GetSiteInstanceForNewTab(
                  params->browser->profile(), url),
              MSG_ROUTING_NONE,
              source_contents,
              NULL);
      // This function takes ownership of |params->target_contents| until it
      // is added to a TabStripModel.
      target_contents_owner.TakeOwnership();
      params->target_contents->extension_tab_helper()->
          SetExtensionAppById(params->extension_app_id);
      // TODO(sky): figure out why this is needed. Without it we seem to get
      // failures in startup tests.
      // By default, content believes it is not hidden.  When adding contents
      // in the background, tell it that it's hidden.
      if ((params->tabstrip_add_types & TabStripModel::ADD_ACTIVE) == 0) {
        // TabStripModel::AddTabContents invokes HideContents if not foreground.
        params->target_contents->web_contents()->WasHidden();
      }
    } else {
      // ... otherwise if we're loading in the current tab, the target is the
      // same as the source.
      params->target_contents = params->source_contents;
      DCHECK(params->target_contents);
    }

    if (user_initiated) {
      params->target_contents->web_contents()->GetRenderViewHost()->
          GetDelegate()->OnUserGesture();
    }

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
    WebContents* target = params->browser->GetWebContentsAt(singleton_index);

    if (target->IsCrashed()) {
      target->GetController().Reload(true);
    } else if (params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE &&
        target->GetURL() != params->url) {
      InitializeExtraHeaders(params, NULL, &extra_headers);
      LoadURLInContents(target, params->url, params, extra_headers);
    }

    // If the singleton tab isn't already selected, select it.
    if (params->source_contents != params->target_contents)
      params->browser->ActivateTabAt(singleton_index, user_initiated);
  }

  if (params->disposition != CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_TAB_ADDED,
        content::Source<content::WebContentsDelegate>(params->browser),
        content::Details<WebContents>(params->target_contents->web_contents()));
  }
}

// Returns the index of an existing singleton tab in |params->browser| matching
// the URL specified in |params|.
int GetIndexOfSingletonTab(browser::NavigateParams* params) {
  if (params->disposition != SINGLETON_TAB)
    return -1;

  // In case the URL was rewritten by the BrowserURLHandler we need to ensure
  // that we do not open another URL that will get redirected to the rewritten
  // URL.
  GURL rewritten_url(params->url);
  bool reverse_on_redirect = false;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url,
      params->browser->profile(),
      &reverse_on_redirect);

  // If there are several matches: prefer the active tab by starting there.
  int start_index = std::max(0, params->browser->active_index());
  int tab_count = params->browser->tab_count();
  for (int i = 0; i < tab_count; ++i) {
    int tab_index = (start_index + i) % tab_count;
    TabContentsWrapper* tab =
        params->browser->GetTabContentsWrapperAt(tab_index);

    url_canon::Replacements<char> replacements;
    if (params->ref_behavior == browser::NavigateParams::IGNORE_REF)
      replacements.ClearRef();
    if (params->path_behavior == browser::NavigateParams::IGNORE_AND_NAVIGATE ||
        params->path_behavior == browser::NavigateParams::IGNORE_AND_STAY_PUT) {
      replacements.ClearPath();
      replacements.ClearQuery();
    }

    if (CompareURLsWithReplacements(tab->web_contents()->GetURL(),
                                    params->url, replacements) ||
        CompareURLsWithReplacements(tab->web_contents()->GetURL(),
                                    rewritten_url, replacements)) {
      params->target_contents = tab;
      return tab_index;
    }
  }

  return -1;
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

}  // namespace browser
