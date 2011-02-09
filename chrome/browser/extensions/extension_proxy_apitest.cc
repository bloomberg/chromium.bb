// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

namespace {

const char NO_SERVER[] = "";
const char NO_PAC[] = "";

}  // namespace

class ProxySettingsApiTest : public ExtensionApiTest {
 protected:
  void ValidateSettings(int expected_mode,
                        const std::string& expected_server,
                        const std::string& expected_pac_url,
                        PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(prefs::kProxy);
    ASSERT_TRUE(pref != NULL);
    EXPECT_TRUE(pref->IsExtensionControlled());

    ProxyConfigDictionary dict(pref_service->GetDictionary(prefs::kProxy));

    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_mode, mode);

    std::string value;
    if (!expected_pac_url.empty()) {
       ASSERT_TRUE(dict.GetPacUrl(&value));
       EXPECT_EQ(expected_pac_url, value);
     } else {
       EXPECT_FALSE(dict.GetPacUrl(&value));
     }

    if (!expected_server.empty()) {
      ASSERT_TRUE(dict.GetProxyServer(&value));
      EXPECT_EQ(expected_server, value);
    } else {
      EXPECT_FALSE(dict.GetProxyServer(&value));
    }
  }

  void ExpectNoSettings(PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(prefs::kProxy);
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
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoPac, pref_service);
}

// Tests auto-detect settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyAutoSettings) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/auto")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_AUTO_DETECT, kNoServer, kNoPac,
                   pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacScript) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/pac")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer,
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
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
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
  ValidateSettings(ProxyPrefs::MODE_SYSTEM, kNoServer, kNoPac, pref_service);
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividual) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("proxy/individual")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=http://1.1.1.1;"
                       "https=socks://2.2.2.2;"
                       "ftp=http://3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
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
  ExpectNoSettings(pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
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
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=http://1.1.1.1;"
                       "https=socks://2.2.2.2;"
                       "ftp=http://3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()->profile()->GetOffTheRecordProfile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
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
  ExpectNoSettings(pref_service);
}
