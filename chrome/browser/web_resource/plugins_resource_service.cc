// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/plugins_resource_service.h"

#include "base/command_line.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
const int kStartResourceFetchDelayMs = 60 * 1000;

// Delay between calls to update the cache 1 day and 2 minutes in testing mode.
const int kCacheUpdateDelayMs = 24 * 60 * 60 * 1000;
const int kTestCacheUpdateDelayMs = 2 * 60 * 1000;

const char kPluginsServerUrl[] =
    "https://www.gstatic.com/chrome/config/plugins_2/";

bool IsTest() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPluginsMetadataServerURL);
}

GURL GetPluginsServerURL() {
  std::string filename;
#if defined(OS_WIN)
  filename = "plugins_win.json";
#elif defined(OS_LINUX)
  filename = "plugins_linux.json";
#elif defined(OS_MACOSX)
  filename = "plugins_mac.json";
#else
  NOTREACHED();
#endif

  std::string test_url =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPluginsMetadataServerURL);
  return GURL(IsTest() ? test_url : kPluginsServerUrl + filename);
}

int GetCacheUpdateDelay() {
  return IsTest() ? kTestCacheUpdateDelayMs : kCacheUpdateDelayMs;
}

}  // namespace

PluginsResourceService::PluginsResourceService(PrefService* local_state)
    : WebResourceService(local_state,
                         GetPluginsServerURL(),
                         false,
                         prefs::kPluginsResourceCacheUpdate,
                         kStartResourceFetchDelayMs,
                         GetCacheUpdateDelay()) {
}

PluginsResourceService::~PluginsResourceService() {
}

// static
void PluginsResourceService::RegisterPrefs(PrefServiceSimple* local_state) {
  local_state->RegisterDictionaryPref(
      prefs::kPluginsMetadata, new base::DictionaryValue());
  local_state->RegisterStringPref(prefs::kPluginsResourceCacheUpdate, "0");
}

void PluginsResourceService::Unpack(const DictionaryValue& parsed_json) {
  prefs_->Set(prefs::kPluginsMetadata, parsed_json);
  PluginFinder::GetInstance()->ReinitializePlugins(parsed_json);
}
