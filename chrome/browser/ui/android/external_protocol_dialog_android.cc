// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/component/navigation_interception/intercept_navigation_delegate.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id, routing_id);
  if (!web_contents)
    return;
  navigation_interception::InterceptNavigationDelegate* delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (!delegate)
    return;

  // TODO(jknotten): The call to ShouldIgnoreNavigation returns false if there
  // are no applications that can handle the given URL. In this case, an error
  // page should be displayed to the user.
  delegate->ShouldIgnoreNavigation(url, true /* has_user_gesture */ );
}
