// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id, routing_id);
  if (web_contents) {
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (tab)
      return tab->RunExternalProtocolDialog(url);
  }
}
