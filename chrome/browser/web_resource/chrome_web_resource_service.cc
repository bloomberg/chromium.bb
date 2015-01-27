// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/chrome_web_resource_service.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_json_parser.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

ChromeWebResourceService::ChromeWebResourceService(
    PrefService* prefs,
    const GURL& web_resource_server,
    bool apply_locale_to_url,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms)
    : web_resource::WebResourceService(
          prefs,
          web_resource_server,
          apply_locale_to_url ? g_browser_process->GetApplicationLocale()
                              : std::string(),
          last_update_time_pref_name,
          start_fetch_delay_ms,
          cache_update_delay_ms,
          g_browser_process->system_request_context(),
          switches::kDisableBackgroundNetworking) {
}

ChromeWebResourceService::~ChromeWebResourceService() {
}

void ChromeWebResourceService::ParseJSON(
    const std::string& data,
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  // SafeJsonParser releases itself on completion.
  scoped_refptr<SafeJsonParser> json_parser(
      new SafeJsonParser(data, success_callback, error_callback));
  json_parser->Start();
}
