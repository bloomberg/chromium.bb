// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/blocked_window_params.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "url/gurl.h"

BlockedWindowParams::BlockedWindowParams(
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const blink::WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    int render_process_id,
    int opener_id)
    : target_url_(target_url),
      referrer_(referrer),
      disposition_(disposition),
      features_(features),
      user_gesture_(user_gesture),
      opener_suppressed_(opener_suppressed),
      render_process_id_(render_process_id),
      opener_id_(opener_id) {
}

chrome::NavigateParams BlockedWindowParams::CreateNavigateParams(
    content::WebContents* web_contents) const {
  GURL popup_url(target_url_);
  web_contents->GetRenderProcessHost()->FilterURL(false, &popup_url);
  chrome::NavigateParams nav_params(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      popup_url,
      content::PAGE_TRANSITION_LINK);
  nav_params.referrer = referrer_;
  nav_params.source_contents = web_contents;
  nav_params.is_renderer_initiated = true;
  nav_params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = user_gesture_;
  nav_params.should_set_opener = !opener_suppressed_;
  nav_params.window_bounds = web_contents->GetContainerBounds();
  if (features_.xSet)
    nav_params.window_bounds.set_x(features_.x);
  if (features_.ySet)
    nav_params.window_bounds.set_y(features_.y);
  if (features_.widthSet)
    nav_params.window_bounds.set_width(features_.width);
  if (features_.heightSet)
    nav_params.window_bounds.set_height(features_.height);

  // Compare RenderViewImpl::show().
  if (!user_gesture_ && disposition_ != NEW_BACKGROUND_TAB)
    nav_params.disposition = NEW_POPUP;
  else
    nav_params.disposition = disposition_;

  return nav_params;
}
