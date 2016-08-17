// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"

class DevToolsAndroidBridgeTest : public InProcessBrowserTest {
};

static void assign_from_callback(scoped_refptr<TCPDeviceProvider>* store,
                                 int* invocation_counter,
                                 scoped_refptr<TCPDeviceProvider> value) {
  (*invocation_counter)++;
  *store = value;
}

IN_PROC_BROWSER_TEST_F(DevToolsAndroidBridgeTest, UpdatesTargetDiscovery) {
  Profile* profile = browser()->profile();

  PrefService* service = profile->GetPrefs();
  service->ClearPref(prefs::kDevToolsTargetDiscoveryConfig);

  DevToolsAndroidBridge* bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile);

  scoped_refptr<TCPDeviceProvider> provider;
  int called = 0;
  bridge->set_tcp_provider_callback_for_test(
      base::Bind(assign_from_callback, &provider, &called));

  EXPECT_LT(0, called);
  EXPECT_EQ(nullptr, provider);

  int invocations = called;
  base::ListValue list;
  list.AppendString("somehost:2000");

  service->Set(prefs::kDevToolsTargetDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  std::set<net::HostPortPair> pairs = provider->get_targets_for_test();
  EXPECT_EQ(1UL, pairs.size());
  net::HostPortPair pair = *pairs.begin();
  EXPECT_EQ(2000, pair.port());
  EXPECT_EQ("somehost", pair.HostForURL());

  invocations = called;
  list.Clear();
  service->Set(prefs::kDevToolsTargetDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_EQ(nullptr, provider);
  invocations = called;

  list.AppendString("b:1");
  list.AppendString("c:2");
  list.AppendString("<not really a good address.");
  list.AppendString("d:3");
  list.AppendString("c:2");
  service->Set(prefs::kDevToolsTargetDiscoveryConfig, list);

  EXPECT_LT(invocations, called);
  EXPECT_NE(nullptr, provider);
  pairs = provider->get_targets_for_test();
  EXPECT_EQ(3UL, pairs.size());
  for (const net::HostPortPair pair : pairs) {
    EXPECT_EQ(pair.port(), pair.HostForURL()[0] - 'a');
  }
}
