// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/webui/web_view_sync_internals_ui.h"

#include "components/sync/driver/about_sync_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewSyncInternalsUI::WebViewSyncInternalsUI(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : SyncInternalsUI(web_ui, host) {}

WebViewSyncInternalsUI::~WebViewSyncInternalsUI() {}

bool WebViewSyncInternalsUI::OverrideHandleWebUIIOSMessage(
    const GURL& source_url,
    const std::string& message,
    const base::ListValue& args) {
  // ios/web_view only supports sync in transport mode. Explicitly override sync
  // start and stop messages and perform a no op.
  return message == syncer::sync_ui_util::kRequestStart ||
         message == syncer::sync_ui_util::kRequestStopKeepData ||
         message == syncer::sync_ui_util::kRequestStopClearData;
}

}  // namespace ios_web_view
