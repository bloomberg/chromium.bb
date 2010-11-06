// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_navigator.h"

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"

namespace {

// Returns the SiteInstance for |source_contents| if it represents the same
// website as |url|, or NULL otherwise. |source_contents| cannot be NULL.
SiteInstance* GetSiteInstance(TabContents* source_contents, const GURL& url) {
  if (!source_contents)
    return NULL;

  // Don't use this logic when "--process-per-tab" is specified.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab) &&
      SiteInstance::IsSameWebSite(source_contents->profile(),
                                  source_contents->GetURL(),
                                  url)) {
    return source_contents->GetSiteInstance();
  }
  return NULL;
}

// Returns true if the specified Browser can open tabs. Not all Browsers support
// multiple tabs, such as app frames and popups. This function returns false for
// those types of Browser.
bool WindowCanOpenTabs(Browser* browser) {
  return browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP) ||
      browser->tabstrip_model()->empty();
}

// Finds an existing Browser compatible with |profile|, making a new one if no
// such Browser is located.
Browser* GetOrCreateBrowser(Profile* profile) {
  Browser* browser = BrowserList::FindBrowserWithType(profile,
                                                      Browser::TYPE_NORMAL,
                                                      false);
  return browser ? browser : Browser::Create(profile);
}

// Returns true if two URLs are equal ignoring their ref (hash fragment).
bool CompareURLsIgnoreRef(const GURL& url, const GURL& other) {
  if (url == other)
    return true;
  // If neither has a ref than there is no point in stripping the refs and
  // the URLs are different since the comparison failed in the previous if
  // statement.
  if (!url.has_ref() && !other.has_ref())
    return false;
  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  GURL url_no_ref = url.ReplaceComponents(replacements);
  GURL other_no_ref = other.ReplaceComponents(replacements);
  return url_no_ref == other_no_ref;
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
  BrowserURLHandler::RewriteURLIfNecessary(&rewritten_url,
                                           params->browser->profile(),
                                           &reverse_on_redirect);

  for (int i = 0; i < params->browser->tab_count(); ++i) {
    TabContents* tab = params->browser->GetTabContentsAt(i);
    if (CompareURLsIgnoreRef(tab->GetURL(), params->url) ||
        CompareURLsIgnoreRef(tab->GetURL(), rewritten_url)) {
      params->target_contents = tab;
      return i;
    }
  }
  return -1;
}

// Returns a Browser that can host the navigation or tab addition specified in
// |params|. This might just return the same Browser specified in |params|, or
// some other if that Browser is deemed incompatible.
Browser* GetBrowserForDisposition(browser::NavigateParams* params) {
  // If no source TabContents was specified, we use the selected one from the
  // target browser. This must happen first, before GetBrowserForDisposition()
  // has a chance to replace |params->browser| with another one.
  if (!params->source_contents && params->browser)
    params->source_contents = params->browser->GetSelectedTabContents();

  Profile* profile =
      params->browser ? params->browser->profile() : params->profile;

  switch (params->disposition) {
    case CURRENT_TAB:
      if (!params->browser && profile) {
        // We specified a profile instead of a browser; find or create one.
        params->browser = Browser::GetOrCreateTabbedBrowser(profile);
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
      // Make a new popup window. Coerce app-style if |params->browser| or the
      // |source| represents an app.
      Browser::Type type = Browser::TYPE_POPUP;
      if ((params->browser && params->browser->type() == Browser::TYPE_APP) ||
          (params->source_contents && params->source_contents->is_app())) {
        type = Browser::TYPE_APP_POPUP;
      }
      if (profile) {
        Browser* browser = new Browser(type, profile);
        browser->set_override_bounds(params->window_bounds);
        browser->CreateBrowserWindow();
        return browser;
      }
      return NULL;
    }
    case NEW_WINDOW:
      // Make a new normal browser window.
      if (profile) {
        Browser* browser = new Browser(Browser::TYPE_NORMAL, profile);
        browser->CreateBrowserWindow();
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
  if (params->browser->tabstrip_model()->empty() &&
      (params->disposition == NEW_BACKGROUND_TAB ||
       params->disposition == CURRENT_TAB ||
       params->disposition == SINGLETON_TAB)) {
    params->disposition = NEW_FOREGROUND_TAB;
  }
  if (params->browser->profile()->IsOffTheRecord() &&
      params->disposition == OFF_THE_RECORD) {
    params->disposition = NEW_FOREGROUND_TAB;
  }

  // Disposition trumps add types. ADD_SELECTED is a default, so we need to
  // remove it if disposition implies the tab is going to open in the
  // background.
  if (params->disposition == NEW_BACKGROUND_TAB)
    params->tabstrip_add_types &= ~TabStripModel::ADD_SELECTED;

  // Code that wants to open a new window typically expects it to be shown
  // automatically.
  if (params->disposition == NEW_WINDOW || params->disposition == NEW_POPUP) {
    params->show_window = true;
    params->tabstrip_add_types |= TabStripModel::ADD_SELECTED;
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
    if (params_->show_window)
      params_->browser->window()->Show();
  }
 private:
  browser::NavigateParams* params_;
  DISALLOW_COPY_AND_ASSIGN(ScopedBrowserDisplayer);
};

// This class manages the lifetime of a TabContents created by the Navigate()
// function. When Navigate() creates a TabContents for a URL, an instance of
// this class takes ownership of it via TakeOwnership() until the TabContents
// is added to a tab strip at which time ownership is relinquished via
// ReleaseOwnership(). If this object goes out of scope without being added
// to a tab strip, the created TabContents is deleted to avoid a leak and the
// params->target_contents field is set to NULL.
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
  TabContents* ReleaseOwnership() {
    return target_contents_owner_.release();
  }

 private:
  browser::NavigateParams* params_;
  scoped_ptr<TabContents> target_contents_owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedTargetContentsOwner);
};

}  // namespace

namespace browser {

NavigateParams::NavigateParams(
    Browser* a_browser,
    const GURL& a_url,
    PageTransition::Type a_transition)
    : url(a_url),
      target_contents(NULL),
      source_contents(NULL),
      disposition(CURRENT_TAB),
      transition(a_transition),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_SELECTED),
      show_window(false),
      browser(a_browser),
      profile(NULL) {
}

NavigateParams::NavigateParams(Browser* a_browser,
                               TabContents* a_target_contents)
    : target_contents(a_target_contents),
      source_contents(NULL),
      disposition(CURRENT_TAB),
      transition(PageTransition::LINK),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_SELECTED),
      show_window(false),
      browser(a_browser),
      profile(NULL) {
}

NavigateParams::~NavigateParams() {
}

void Navigate(NavigateParams* params) {
  params->browser = GetBrowserForDisposition(params);
  if (!params->browser)
    return;
  // Navigate() must not return early after this point.

  // Make sure the Browser is shown if params call for it.
  ScopedBrowserDisplayer displayer(params);

  // Makes sure any TabContents created by this function is destroyed if
  // not properly added to a tab strip.
  ScopedTargetContentsOwner target_contents_owner(params);

  // Some dispositions need coercion to base types.
  NormalizeDisposition(params);

  // Determine if the navigation was user initiated. If it was, we need to
  // inform the target TabContents, and we may need to update the UI.
  PageTransition::Type base_transition =
      PageTransition::StripQualifier(params->transition);
  bool user_initiated = base_transition == PageTransition::TYPED ||
                        base_transition == PageTransition::AUTO_BOOKMARK;

  // If no target TabContents was specified, we need to construct one if we are
  // supposed to target a new tab.
  if (!params->target_contents) {
    if (params->disposition != CURRENT_TAB) {
      params->target_contents =
          new TabContents(params->browser->profile(),
                          GetSiteInstance(params->source_contents, params->url),
                          MSG_ROUTING_NONE,
                          params->source_contents,
                          NULL);
      // This function takes ownership of |params->target_contents| until it
      // is added to a TabStripModel.
      target_contents_owner.TakeOwnership();
      params->target_contents->SetExtensionAppById(params->extension_app_id);
      // TODO(sky): figure out why this is needed. Without it we seem to get
      // failures in startup tests.
      // By default, content believes it is not hidden.  When adding contents
      // in the background, tell it that it's hidden.
      if ((params->tabstrip_add_types & TabStripModel::ADD_SELECTED) == 0) {
        // TabStripModel::AddTabContents invokes HideContents if not foreground.
        params->target_contents->WasHidden();
      }
    } else {
      // ... otherwise if we're loading in the current tab, the target is the
      // same as the source.
      params->target_contents = params->source_contents;
      DCHECK(params->target_contents);
    }

    if (user_initiated) {
      RenderViewHostDelegate::BrowserIntegration* integration =
          params->target_contents;
      integration->OnUserGesture();
    }

    // Perform the actual navigation.
    GURL url = params->url.is_empty() ? params->browser->GetHomePage()
                                      : params->url;
    params->target_contents->controller().LoadURL(url, params->referrer,
                                                  params->transition);
  } else {
    // |target_contents| was specified non-NULL, and so we assume it has already
    // been navigated appropriately. We need to do nothing more other than
    // add it to the appropriate tabstrip.
  }

  if (params->source_contents == params->target_contents) {
    // The navigation occurred in the source tab, so update the UI.
    params->browser->UpdateUIForNavigationInTab(params->target_contents,
                                                params->transition,
                                                user_initiated);
  } else {
    // The navigation occurred in some other tab.
    int singleton_index = GetIndexOfSingletonTab(params);
    if (params->disposition == SINGLETON_TAB && singleton_index >= 0) {
      // The navigation should re-select an existing tab in the target Browser.
      params->browser->SelectTabContentsAt(singleton_index, user_initiated);
    } else {
      // If some non-default value is set for the index, we should tell the
      // TabStripModel to respect it.
      if (params->tabstrip_index != -1)
        params->tabstrip_add_types |= TabStripModel::ADD_FORCE_INDEX;

      // The navigation should insert a new tab into the target Browser.
      params->browser->tabstrip_model()->AddTabContents(
          params->target_contents,
          params->tabstrip_index,
          params->transition,
          params->tabstrip_add_types);
      // Now that the |params->target_contents| is safely owned by the target
      // Browser's TabStripModel, we can release ownership.
      target_contents_owner.ReleaseOwnership();
    }
  }
}

}  // namespace browser
