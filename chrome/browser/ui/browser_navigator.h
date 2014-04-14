// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace chrome {

// Parameters that tell Navigate() what to do.
//
// Some basic examples:
//
// Simple Navigate to URL in current tab:
// chrome::NavigateParams params(browser, GURL("http://www.google.com/"),
//                               content::PAGE_TRANSITION_LINK);
// chrome::Navigate(&params);
//
// Open bookmark in new background tab:
// chrome::NavigateParams params(browser, url,
//                               content::PAGE_TRANSITION_AUTO_BOOKMARK);
// params.disposition = NEW_BACKGROUND_TAB;
// chrome::Navigate(&params);
//
// Opens a popup WebContents:
// chrome::NavigateParams params(browser, popup_contents);
// params.source_contents = source_contents;
// chrome::Navigate(&params);
//
// See browser_navigator_browsertest.cc for more examples.
//
struct NavigateParams {
  NavigateParams(Browser* browser,
                 const GURL& a_url,
                 content::PageTransition a_transition);
  NavigateParams(Browser* browser,
                 content::WebContents* a_target_contents);
  NavigateParams(Profile* profile,
                 const GURL& a_url,
                 content::PageTransition a_transition);
  ~NavigateParams();

  // The URL/referrer to be loaded. Ignored if |target_contents| is non-NULL.
  GURL url;
  content::Referrer referrer;

  // The browser-global ID of the frame to navigate, or -1 for the main frame.
  int64 frame_tree_node_id;

  // Any redirect URLs that occurred for this navigation before |url|.
  // Usually empty.
  std::vector<GURL> redirect_chain;

  // Indicates whether this navigation will be sent using POST.
  // The POST method is limited support for basic POST data by leveraging
  // NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST.
  // It is not for things like file uploads.
  bool uses_post;

  // The post data when the navigation uses POST.
  scoped_refptr<base::RefCountedMemory> browser_initiated_post_data;

  // Extra headers to add to the request for this page.  Headers are
  // represented as "<name>: <value>" and separated by \r\n.  The entire string
  // is terminated by \r\n.  May be empty if no extra headers are needed.
  std::string extra_headers;

  // [in]  A WebContents to be navigated or inserted into the target
  //       Browser's tabstrip. If NULL, |url| or the homepage will be used
  //       instead. When non-NULL, Navigate() assumes it has already been
  //       navigated to its intended destination and will not load any URL in it
  //       (i.e. |url| is ignored).
  //       Default is NULL.
  // [out] The WebContents in which the navigation occurred or that was
  //       inserted. Guaranteed non-NULL except for note below:
  // Note: If this field is set to NULL by the caller and Navigate() creates
  //       a new WebContents, this field will remain NULL and the
  //       WebContents deleted if the WebContents it created is
  //       not added to a TabStripModel before Navigate() returns.
  content::WebContents* target_contents;

  // [in]  The WebContents that initiated the Navigate() request if such
  //       context is necessary. Default is NULL, i.e. no context.
  // [out] If NULL, this value will be set to the selected WebContents in
  //       the originating browser prior to the operation performed by
  //       Navigate(). However, if the originating page is from a different
  //       profile (e.g. an OFF_THE_RECORD page originating from a non-OTR
  //       window), then |source_contents| is reset to NULL.
  content::WebContents* source_contents;

  // The disposition requested by the navigation source. Default is
  // CURRENT_TAB. What follows is a set of coercions that happen to this value
  // when other factors are at play:
  //
  // [in]:                Condition:                        [out]:
  // NEW_BACKGROUND_TAB   target browser tabstrip is empty  NEW_FOREGROUND_TAB
  // CURRENT_TAB          "     "     "                     NEW_FOREGROUND_TAB
  // OFF_THE_RECORD       target browser profile is incog.  NEW_FOREGROUND_TAB
  //
  // If disposition is NEW_BACKGROUND_TAB, TabStripModel::ADD_ACTIVE is
  // removed from |tabstrip_add_types| automatically.
  // If disposition is one of NEW_WINDOW, NEW_POPUP, NEW_FOREGROUND_TAB or
  // SINGLETON_TAB, then TabStripModel::ADD_ACTIVE is automatically added to
  // |tabstrip_add_types|.
  WindowOpenDisposition disposition;

  // Sets browser->is_trusted_source. Default is false.
  bool trusted_source;

  // The transition type of the navigation. Default is
  // content::PAGE_TRANSITION_LINK when target_contents is specified in the
  // constructor.
  content::PageTransition transition;

  // Whether this navigation was initiated by the renderer process. Default is
  // false.
  bool is_renderer_initiated;

  // The index the caller would like the tab to be positioned at in the
  // TabStrip. The actual index will be determined by the TabHandler in
  // accordance with |add_types|. Defaults to -1 (allows the TabHandler to
  // decide).
  int tabstrip_index;

  // A bitmask of values defined in TabStripModel::AddTabTypes. Helps
  // determine where to insert a new tab and whether or not it should be
  // selected, among other properties. Default is ADD_ACTIVE.
  int tabstrip_add_types;

  // If non-empty, the new tab is an app tab.
  std::string extension_app_id;

  // If non-empty, specifies the desired initial position and size of the
  // window if |disposition| == NEW_POPUP.
  // TODO(beng): Figure out if this can be used to create Browser windows
  //             for other callsites that use set_override_bounds, or
  //             remove this comment.
  gfx::Rect window_bounds;

  // Determines if and how the target window should be made visible at the end
  // of the call to Navigate().
  enum WindowAction {
    // Do not show or activate the browser window after navigating.
    NO_ACTION,
    // Show and activate the browser window after navigating.
    SHOW_WINDOW,
    // Show the browser window after navigating but do not activate.
    SHOW_WINDOW_INACTIVE
  };
  // Default is NO_ACTION (don't show or activate the window).
  // If disposition is NEW_WINDOW or NEW_POPUP, and |window_action| is set to
  // NO_ACTION, |window_action| will be set to SHOW_WINDOW.
  WindowAction window_action;

  // If false then the navigation was not initiated by a user gesture.
  // Default is true.
  bool user_gesture;

  // What to do with the path component of the URL for singleton navigations.
  enum PathBehavior {
    // Two URLs with differing paths are different.
    RESPECT,
    // Ignore path when finding existing tab, navigate to new URL.
    IGNORE_AND_NAVIGATE,
    // Ignore path when finding existing tab, don't navigate tab.
    IGNORE_AND_STAY_PUT,
  };
  // Default is RESPECT.
  PathBehavior path_behavior;

  // What to do with the ref component of the URL for singleton navigations.
  enum RefBehavior {
    // Two URLs with differing refs are same.
    IGNORE_REF,
    // Two URLs with differing refs are different.
    RESPECT_REF,
  };
  // Default is IGNORE.
  RefBehavior ref_behavior;

  // [in]  Specifies a Browser object where the navigation could occur or the
  //       tab could be added. Navigate() is not obliged to use this Browser if
  //       it is not compatible with the operation being performed. This can be
  //       NULL, in which case |initiating_profile| must be provided.
  // [out] Specifies the Browser object where the navigation occurred or the
  //       tab was added. Guaranteed non-NULL unless the disposition did not
  //       require a navigation, in which case this is set to NULL
  //       (SUPPRESS_OPEN, SAVE_TO_DISK, IGNORE_ACTION).
  // Note: If |show_window| is set to false and a new Browser is created by
  //       Navigate(), the caller is responsible for showing it so that its
  //       window can assume responsibility for the Browser's lifetime (Browser
  //       objects are deleted when the user closes a visible browser window).
  Browser* browser;

  // The profile that is initiating the navigation. If there is a non-NULL
  // browser passed in via |browser|, it's profile will be used instead.
  Profile* initiating_profile;

  // Refers to a navigation that was parked in the browser in order to be
  // transferred to another RVH. Only used in case of a redirection of a request
  // to a different site that created a new RVH.
  content::GlobalRequestID transferred_global_request_id;

  // Refers to which desktop this navigation should occur on. May be passed
  // explicitly or inferred from an existing Browser instance.
  chrome::HostDesktopType host_desktop_type;

  // Indicates whether this navigation  should replace the current
  // navigation entry.
  bool should_replace_current_entry;

  // Indicates whether |source_contents| should be set as opener when creating
  // |target_contents|.
  bool should_set_opener;

 private:
  NavigateParams();
};

// Copies fields from |params| struct to |nav_params| struct.
void FillNavigateParamsFromOpenURLParams(chrome::NavigateParams* nav_params,
                                         const content::OpenURLParams& params);

// Navigates according to the configuration specified in |params|.
void Navigate(NavigateParams* params);

// Returns true if the url is allowed to open in incognito window.
bool IsURLAllowedInIncognito(const GURL& url,
                             content::BrowserContext* browser_context);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
