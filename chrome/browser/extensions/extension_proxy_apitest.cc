// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

class ProxySettingsApiTest : public ExtensionApiTest {
 protected:
  void AssertSettings(int expected_mode,
                      const char* expected_server,
                      const char* expected_pac_url,
                      PrefService* pref_service) {
    AssertExtensionControlled(prefs::kProxyMode, pref_service);
    int mode = pref_service->GetInteger(prefs::kProxyMode);
    EXPECT_EQ(expected_mode, mode);

    AssertExtensionControlled(prefs::kProxyPacUrl, pref_service);
    EXPECT_EQ(expected_pac_url, pref_service->GetString(prefs::kProxyPacUrl));

    AssertExtensionControlled(prefs::kProxyServer, pref_service);
    EXPECT_EQ(expected_server, pref_service->GetString(prefs::kProxyServer));
  }

  void AssertNoSettings(PrefService* pref_service) {
    AssertNotExtensionControlled(prefs::kProxyServer, pref_service);
    AssertNotExtensionControlled(prefs::kProxyMode, pref_service);
    AssertNotExtensionControlled(prefs::kProxyPacUrl, pref_service);
  }
 private:
  void AssertExtensionControlled(const char* pref_key,
                                 PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(pref_key);
    ASSERT_TRUE(pref != NULL);
    EXPECT_TRUE(pref->IsExtensionControlled());
  }

  void AssertNotExtensionControlled(const char* pref_key,
                                    PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(pref_key);
    ASSERT_TRUE(pref != NULL);
    EXPECT_FALSE(pref->IsExtensionControlled());
  }
};

namespace {

const char kNoServer[] = "";
const char kNoPac[] = "";

}  // namespace

// Tests direct connection settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyDirectSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/direct")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoPac, pref_service);
}

// Tests auto-detect settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyAutoSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/auto")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_AUTO_DETECT, kNoServer, kNoPac, pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacScript) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/pac")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer,
                 "http://wpad/windows.pac", pref_service);
}

// Tests setting a single proxy to cover all schemes.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedSingle) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/single")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://127.0.0.1:100;"
                     "https=http://127.0.0.1:100;"
                     "ftp=http://127.0.0.1:100;"
                     "socks=http://9.9.9.9",
                 kNoPac,
                 pref_service);
}

// Tests setting to use the system's proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxySystem) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/system")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_SYSTEM, kNoServer, kNoPac, pref_service);
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividual) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://1.1.1.1;"
                     "https=socks://2.2.2.2;"
                     "ftp=http://3.3.3.3:9000;"
                     "socks=socks4://4.4.4.4:9090",
                 kNoPac,
                 pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://1.1.1.1;"
                     "https=socks://2.2.2.2;"
                     "ftp=http://3.3.3.3:9000;"
                     "socks=socks4://4.4.4.4:9090",
                 kNoPac,
                 pref_service);
}

// Tests setting values only for incognito mode
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
    ProxyFixedIndividualIncognitoOnly) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual_incognito_only")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertNoSettings(pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://1.1.1.1;"
                     "https=socks://2.2.2.2;"
                     "ftp=http://3.3.3.3:9000;"
                     "socks=socks4://4.4.4.4:9090",
                 kNoPac,
                 pref_service);
}

// Tests setting values also for incognito mode
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
    ProxyFixedIndividualIncognitoAlso) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual_incognito_also")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://1.1.1.1;"
                     "https=socks://2.2.2.2;"
                     "ftp=http://3.3.3.3:9000;"
                     "socks=socks4://4.4.4.4:9090",
                 kNoPac,
                 pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  AssertSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "http=http://5.5.5.5;"
                     "https=socks://6.6.6.6;"
                     "ftp=http://7.7.7.7:9000;"
                     "socks=socks4://8.8.8.8:9090",
                 kNoPac,
                 pref_service);
}

// Tests setting and unsetting values
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividualRemove) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual_remove")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  AssertNoSettings(pref_service);
}
