// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

// Tests setting a single proxy to cover all schemes.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProxySingle) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/single")) << message_;
  Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();

  // There should be no values superseding the extension-set proxy in this test.
  std::string proxy_server = pref_service->GetString(prefs::kProxyServer);

  ASSERT_EQ("http=http://127.0.0.1:100;"
            "https=http://127.0.0.1:100;"
            "ftp=http://127.0.0.1:100;"
            "socks=http://9.9.9.9", proxy_server);
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProxyIndividual) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual")) << message_;
  Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();

  // There should be no values superseding the extension-set proxy in this test.
  std::string proxy_server = pref_service->GetString(prefs::kProxyServer);

  ASSERT_EQ("http=http://1.1.1.1;"
            "https=socks://2.2.2.2;"
            "ftp=http://3.3.3.3:9000;"
            "socks=socks4://4.4.4.4:9090", proxy_server);
}
