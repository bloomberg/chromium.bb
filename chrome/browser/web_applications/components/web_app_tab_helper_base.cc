// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"

#include "content/public/browser/navigation_handle.h"

namespace web_app {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelperBase)

WebAppTabHelperBase::WebAppTabHelperBase(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebAppTabHelperBase::~WebAppTabHelperBase() = default;

void WebAppTabHelperBase::SetAppId(const AppId& app_id) {
  app_id_ = app_id;
}

void WebAppTabHelperBase::ResetAppId() {
  app_id_.clear();
}

void WebAppTabHelperBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  const AppId app_id = GetAppId(navigation_handle->GetURL());
  SetAppId(app_id);
}

void WebAppTabHelperBase::DidCloneToNewWebContents(
    content::WebContents* old_web_contents,
    content::WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a WebAppTabHelperBase.
  WebAppTabHelperBase* new_tab_helper = CloneForWebContents(new_web_contents);
  // Clone common state:
  new_tab_helper->SetAppId(app_id());
}

}  // namespace web_app
