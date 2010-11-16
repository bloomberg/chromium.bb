// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

// Tests auto-detect and PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProxyAutoSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/auto")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();

  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxyAutoDetect);
  ASSERT_TRUE(pref != NULL);
  ASSERT_TRUE(pref->IsExtensionControlled());
  bool auto_detect = pref_service->GetBoolean(prefs::kProxyAutoDetect);
  EXPECT_TRUE(auto_detect);

  pref = pref_service->FindPreference(prefs::kProxyPacUrl);
  ASSERT_TRUE(pref != NULL);
  ASSERT_TRUE(pref->IsExtensionControlled());
  std::string pac_url = pref_service->GetString(prefs::kProxyPacUrl);
  EXPECT_EQ("http://wpad/windows.pac", pac_url);

  // No manual proxy prefs were set.
  pref = pref_service->FindPreference(prefs::kProxyServer);
  ASSERT_TRUE(pref != NULL);
  EXPECT_TRUE(pref->IsDefaultValue());
}

// Tests setting a single proxy to cover all schemes.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProxyManualSingle) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/single")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();

  // There should be no values superseding the extension-set proxy in this test.
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxyServer);
  ASSERT_TRUE(pref != NULL);
  ASSERT_TRUE(pref->IsExtensionControlled());
  std::string proxy_server = pref_service->GetString(prefs::kProxyServer);
  EXPECT_EQ("http=http://127.0.0.1:100;"
            "https=http://127.0.0.1:100;"
            "ftp=http://127.0.0.1:100;"
            "socks=http://9.9.9.9", proxy_server);

  // Other proxy prefs should also be set, so they're all controlled from one
  // place.
  pref = pref_service->FindPreference(prefs::kProxyAutoDetect);
  ASSERT_TRUE(pref != NULL);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProxyAutoDetect));

  pref = pref_service->FindPreference(prefs::kProxyPacUrl);
  ASSERT_TRUE(pref != NULL);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_EQ("", pref_service->GetString(prefs::kProxyPacUrl));
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ProxyManualIndividual) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();

  // There should be no values superseding the extension-set proxy in this test.
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxyServer);
  ASSERT_TRUE(pref != NULL);
  ASSERT_TRUE(pref->IsExtensionControlled());

  std::string proxy_server = pref_service->GetString(prefs::kProxyServer);
  EXPECT_EQ("http=http://1.1.1.1;"
            "https=socks://2.2.2.2;"
            "ftp=http://3.3.3.3:9000;"
            "socks=socks4://4.4.4.4:9090", proxy_server);

  // Other proxy prefs should also be set, so they're all controlled from one
  // place.
  pref = pref_service->FindPreference(prefs::kProxyAutoDetect);
  ASSERT_TRUE(pref != NULL);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_FALSE(pref_service->GetBoolean(prefs::kProxyAutoDetect));

  pref = pref_service->FindPreference(prefs::kProxyPacUrl);
  ASSERT_TRUE(pref != NULL);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_EQ("", pref_service->GetString(prefs::kProxyPacUrl));
}
