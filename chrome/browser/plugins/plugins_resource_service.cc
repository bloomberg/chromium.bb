// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_resource_service.h"

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "url/gurl.h"

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
#error Unknown platform
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
