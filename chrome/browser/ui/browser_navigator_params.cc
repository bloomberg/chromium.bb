// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator_params.h"

#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;

namespace chrome {

#if defined(OS_ANDROID)
NavigateParams::NavigateParams(WebContents* a_target_contents)
    : frame_tree_node_id(-1),
      uses_post(false),
      target_contents(a_target_contents),
      source_contents(nullptr),
      disposition(WindowOpenDisposition::CURRENT_TAB),
      trusted_source(false),
      transition(ui::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(NO_ACTION),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
      initiating_profile(nullptr),
      should_replace_current_entry(false),
      created_with_opener(false),
      started_from_context_menu(false) {}
#else
NavigateParams::NavigateParams(Browser* a_browser,
                               const GURL& a_url,
                               ui::PageTransition a_transition)
    : url(a_url),
      frame_tree_node_id(-1),
      uses_post(false),
      target_contents(NULL),
      source_contents(NULL),
      disposition(WindowOpenDisposition::CURRENT_TAB),
      trusted_source(false),
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
      should_replace_current_entry(false),
      created_with_opener(false),
      started_from_context_menu(false) {}

NavigateParams::NavigateParams(Browser* a_browser,
                               WebContents* a_target_contents)
    : frame_tree_node_id(-1),
      uses_post(false),
      target_contents(a_target_contents),
      source_contents(NULL),
      disposition(WindowOpenDisposition::CURRENT_TAB),
      trusted_source(false),
      transition(ui::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(NO_ACTION),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
      browser(a_browser),
      initiating_profile(NULL),
      should_replace_current_entry(false),
      created_with_opener(false),
      started_from_context_menu(false) {}
#endif  // !defined(OS_ANDROID)

NavigateParams::NavigateParams(Profile* a_profile,
                               const GURL& a_url,
                               ui::PageTransition a_transition)
    : url(a_url),
      frame_tree_node_id(-1),
      uses_post(false),
      target_contents(NULL),
      source_contents(NULL),
      disposition(WindowOpenDisposition::NEW_FOREGROUND_TAB),
      trusted_source(false),
      transition(a_transition),
      is_renderer_initiated(false),
      tabstrip_index(-1),
      tabstrip_add_types(TabStripModel::ADD_ACTIVE),
      window_action(SHOW_WINDOW),
      user_gesture(true),
      path_behavior(RESPECT),
      ref_behavior(IGNORE_REF),
#if !defined(OS_ANDROID)
      browser(NULL),
#endif
      initiating_profile(a_profile),
      should_replace_current_entry(false),
      created_with_opener(false),
      started_from_context_menu(false) {
}

NavigateParams::NavigateParams(const NavigateParams& other) = default;

NavigateParams::~NavigateParams() {}

void FillNavigateParamsFromOpenURLParams(NavigateParams* nav_params,
                                         const content::OpenURLParams& params) {
  nav_params->referrer = params.referrer;
  nav_params->source_site_instance = params.source_site_instance;
  nav_params->frame_tree_node_id = params.frame_tree_node_id;
  nav_params->redirect_chain = params.redirect_chain;
  nav_params->extra_headers = params.extra_headers;
  nav_params->disposition = params.disposition;
  nav_params->trusted_source = false;
  nav_params->is_renderer_initiated = params.is_renderer_initiated;
  nav_params->should_replace_current_entry =
      params.should_replace_current_entry;
  nav_params->uses_post = params.uses_post;
  nav_params->post_data = params.post_data;
  nav_params->started_from_context_menu = params.started_from_context_menu;
}

}  // namespace chrome
