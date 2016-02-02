// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_resource_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_json/safe_json_parser.h"
#include "url/gurl.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
const int kStartResourceFetchDelayMs = 60 * 1000;

// Delay between calls to update the cache 1 day and 2 minutes in testing mode.
const int kCacheUpdateDelayMs = 24 * 60 * 60 * 1000;

const char kPluginsServerUrl[] =
    "https://www.gstatic.com/chrome/config/plugins_2/";

GURL GetPluginsServerURL() {
  std::string filename;
#if defined(OS_WIN)
  filename = "plugins_win.json";
#elif defined(OS_LINUX)
  filename = "plugins_linux.json";
#elif defined(OS_MACOSX)
  filename = "plugins_mac.json";
#else
#error Unknown platform
#endif

  return GURL(kPluginsServerUrl + filename);
}

}  // namespace

PluginsResourceService::PluginsResourceService(PrefService* local_state)
    : web_resource::WebResourceService(
          local_state,
          GetPluginsServerURL(),
          std::string(),
          prefs::kPluginsResourceCacheUpdate,
          kStartResourceFetchDelayMs,
          kCacheUpdateDelayMs,
          g_browser_process->system_request_context(),
          switches::kDisableBackgroundNetworking,
          base::Bind(safe_json::SafeJsonParser::Parse)) {}

void PluginsResourceService::Init() {
  const base::DictionaryValue* metadata =
      prefs_->GetDictionary(prefs::kPluginsMetadata);
  PluginFinder::GetInstance()->ReinitializePlugins(metadata);
  StartAfterDelay();
}

PluginsResourceService::~PluginsResourceService() {
}

// static
void PluginsResourceService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kPluginsMetadata,
                                   new base::DictionaryValue());
  registry->RegisterStringPref(prefs::kPluginsResourceCacheUpdate, "0");
}

void PluginsResourceService::Unpack(const base::DictionaryValue& parsed_json) {
  prefs_->Set(prefs::kPluginsMetadata, parsed_json);
  PluginFinder::GetInstance()->ReinitializePlugins(&parsed_json);
}
