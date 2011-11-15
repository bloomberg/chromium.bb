// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_server_properties_manager.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;
using content::BrowserThread;

class TestingHttpServerPropertiesManager : public HttpServerPropertiesManager {
 public:
  explicit TestingHttpServerPropertiesManager(PrefService* pref_service)
      : HttpServerPropertiesManager(pref_service) {
    InitializeOnIOThread();
  }

  virtual ~TestingHttpServerPropertiesManager() {
  }

  // Make these methods public for testing.
  using HttpServerPropertiesManager::ScheduleUpdateCacheOnUI;
  using HttpServerPropertiesManager::ScheduleUpdatePrefsOnIO;

  // Post tasks without a delay during tests.
  virtual void StartPrefsUpdateTimerOnIO(base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartPrefsUpdateTimerOnIO(
        base::TimeDelta());
  }

  void UpdateCacheFromPrefsOnUIConcrete() {
    HttpServerPropertiesManager::UpdateCacheFromPrefsOnUI();
  }

  // Post tasks without a delay during tests.
  virtual void StartCacheUpdateTimerOnUI(base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartCacheUpdateTimerOnUI(
        base::TimeDelta());
  }

  void UpdatePrefsFromCacheOnIOConcrete() {
    HttpServerPropertiesManager::UpdatePrefsFromCacheOnIO();
  }

  MOCK_METHOD0(UpdateCacheFromPrefsOnUI, void());
  MOCK_METHOD0(UpdatePrefsFromCacheOnIO, void());
  MOCK_METHOD3(UpdateCacheFromPrefsOnIO,
               void(std::vector<std::string>* spdy_servers,
                    net::SpdySettingsMap* spdy_settings_map,
                    net::AlternateProtocolMap* alternate_protocol_map));
  MOCK_METHOD3(UpdatePrefsOnUI,
               void(base::ListValue* spdy_server_list,
                    net::SpdySettingsMap* spdy_settings_map,
                    net::AlternateProtocolMap* alternate_protocol_map));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingHttpServerPropertiesManager);
};

class HttpServerPropertiesManagerTest : public testing::Test {
 protected:
  HttpServerPropertiesManagerTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    pref_service_.RegisterDictionaryPref(prefs::kHttpServerProperties);
    http_server_props_manager_.reset(
        new StrictMock<TestingHttpServerPropertiesManager>(&pref_service_));
    ExpectCacheUpdate();
    loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    if (http_server_props_manager_.get())
      http_server_props_manager_->ShutdownOnUIThread();
    loop_.RunAllPending();
    // Delete |http_server_props_manager_| while |io_thread_| is mapping IO to
    // |loop_|.
    http_server_props_manager_.reset();
  }

  void ExpectCacheUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdateCacheFromPrefsOnUI())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdateCacheFromPrefsOnUIConcrete));
  }

  void ExpectPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdatePrefsFromCacheOnIO())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdatePrefsFromCacheOnIOConcrete));
  }

  MessageLoop loop_;
  TestingPrefService pref_service_;
  scoped_ptr<TestingHttpServerPropertiesManager> http_server_props_manager_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest,
       SingleUpdateForTwoSpdyServerPrefChanges) {
  ExpectCacheUpdate();

  // Set up the prefs for www.google.com:80 and mail.google.com:80 and then set
  // it twice. Only expect a single cache update.

  base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

  // Set supports_spdy for www.google.com:80.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up the SpdySettings for www.google.com:80.
  base::ListValue* spdy_settings_list = new base::ListValue;
  base::DictionaryValue* spdy_setting_dict = new base::DictionaryValue;
  spdy_setting_dict->SetInteger("flags", spdy::SETTINGS_FLAG_PERSISTED);
  spdy_setting_dict->SetInteger("id", 1234);
  spdy_setting_dict->SetInteger("value", 31337);
  spdy_settings_list->Append(spdy_setting_dict);
  server_pref_dict->Set("settings", spdy_settings_list);

  // Set up alternate_protocol for www.google.com:80.
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 443);
  alternate_protocol->SetInteger("protocol", static_cast<int>(net::NPN_SPDY_1));
  server_pref_dict->SetWithoutPathExpansion(
      "alternate_protocol", alternate_protocol);

  // Set the server preference for www.google.com:80.
  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  http_server_properties_dict->SetWithoutPathExpansion(
      "www.google.com:80", server_pref_dict);

  // Set the preference for mail.google.com server.
  base::DictionaryValue* server_pref_dict1 = new base::DictionaryValue;

  // Set supports_spdy for mail.google.com:80
  server_pref_dict1->SetBoolean("supports_spdy", true);

  // Set up the SpdySettings for mail.google.com:80
  base::ListValue* spdy_settings_list1 = new base::ListValue;
  base::DictionaryValue* spdy_setting_dict1 = new base::DictionaryValue;
  spdy_setting_dict1->SetInteger("flags", spdy::SETTINGS_FLAG_PERSISTED);
  spdy_setting_dict1->SetInteger("id", 5678);
  spdy_setting_dict1->SetInteger("value", 62667);
  spdy_settings_list1->Append(spdy_setting_dict1);
  server_pref_dict1->Set("settings", spdy_settings_list1);

  // Set up alternate_protocol for mail.google.com:80
  base::DictionaryValue* alternate_protocol1 = new base::DictionaryValue;
  alternate_protocol1->SetInteger("port", 444);
  alternate_protocol1->SetInteger(
      "protocol", static_cast<int>(net::NPN_SPDY_2));
  server_pref_dict1->SetWithoutPathExpansion(
      "alternate_protocol", alternate_protocol1);

  // Set the server preference for mail.google.com:80.
  http_server_properties_dict->SetWithoutPathExpansion(
      "mail.google.com:80", server_pref_dict1);

  // Set the same value for kHttpServerProperties multiple times.
  pref_service_.SetManagedPref(prefs::kHttpServerProperties,
                               http_server_properties_dict);
  base::DictionaryValue* http_server_properties_dict2 =
      http_server_properties_dict->DeepCopy();
  pref_service_.SetManagedPref(prefs::kHttpServerProperties,
                               http_server_properties_dict2);

  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify SupportsSpdy.
  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("www.google.com:80")));
  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("mail.google.com:80")));
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("foo.google.com:1337")));

  // Verify SpdySettings.
  spdy::SpdySettings spdy_settings_ret =
      http_server_props_manager_->GetSpdySettings(
          net::HostPortPair::FromString("www.google.com:80"));
  ASSERT_EQ(1U, spdy_settings_ret.size());
  spdy::SpdySetting spdy_setting1_ret = spdy_settings_ret.front();
  spdy::SettingsFlagsAndId id1_ret(spdy_setting1_ret.first);
  EXPECT_EQ(1234U, id1_ret.id());
  EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, id1_ret.flags());
  EXPECT_EQ(31337u, spdy_setting1_ret.second);
  spdy_settings_ret = http_server_props_manager_->GetSpdySettings(
          net::HostPortPair::FromString("mail.google.com:80"));
  ASSERT_EQ(1U, spdy_settings_ret.size());
  spdy_setting1_ret = spdy_settings_ret.front();
  id1_ret = spdy_setting1_ret.first;
  EXPECT_EQ(5678U, id1_ret.id());
  EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, id1_ret.flags());
  EXPECT_EQ(62667u, spdy_setting1_ret.second);

  // Verify AlternateProtocol.
  ASSERT_TRUE(http_server_props_manager_->HasAlternateProtocol(
      net::HostPortPair::FromString("www.google.com:80")));
  ASSERT_TRUE(http_server_props_manager_->HasAlternateProtocol(
      net::HostPortPair::FromString("mail.google.com:80")));
  net::PortAlternateProtocolPair port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(
          net::HostPortPair::FromString("www.google.com:80"));
  EXPECT_EQ(443, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_1, port_alternate_protocol.protocol);
  port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(
          net::HostPortPair::FromString("mail.google.com:80"));
  EXPECT_EQ(444, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_2, port_alternate_protocol.protocol);
}

TEST_F(HttpServerPropertiesManagerTest, SupportsSpdy) {
  ExpectPrefsUpdate();

  // Post an update task to the IO thread. SetSupportsSpdy calls
  // ScheduleUpdatePrefsOnIO.

  // Add mail.google.com:443 as a supporting spdy server.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);

  // Run the task.
  loop_.RunAllPending();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, SetSpdySettings) {
  ExpectPrefsUpdate();

  // Add SpdySettings for mail.google.com:443.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  spdy::SpdySettings spdy_settings;
  spdy::SettingsFlagsAndId id1(0);
  id1.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  id1.set_id(1234);
  spdy_settings.push_back(std::make_pair(id1, 31337));
  http_server_props_manager_->SetSpdySettings(spdy_server_mail, spdy_settings);

  // Run the task.
  loop_.RunAllPending();

  spdy::SpdySettings spdy_settings_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, spdy_settings_ret.size());
  spdy::SpdySetting spdy_setting1_ret = spdy_settings_ret.front();
  spdy::SettingsFlagsAndId id1_ret(spdy_setting1_ret.first);
  EXPECT_EQ(1234U, id1_ret.id());
  EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, id1_ret.flags());
  EXPECT_EQ(31337U, spdy_setting1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, HasAlternateProtocol) {
  ExpectPrefsUpdate();

  net::HostPortPair spdy_server_mail("mail.google.com", 80);
  EXPECT_FALSE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  http_server_props_manager_->SetAlternateProtocol(
      spdy_server_mail, 443, net::NPN_SPDY_2);

  // Run the task.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ASSERT_TRUE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  net::PortAlternateProtocolPair port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(spdy_server_mail);
  EXPECT_EQ(443, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_2, port_alternate_protocol.protocol);
}

TEST_F(HttpServerPropertiesManagerTest, Clear) {
  ExpectPrefsUpdate();

  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);
  http_server_props_manager_->SetAlternateProtocol(
      spdy_server_mail, 443, net::NPN_SPDY_2);

  spdy::SpdySettings spdy_settings;
  spdy::SettingsFlagsAndId id1(0);
  id1.set_flags(spdy::SETTINGS_FLAG_PLEASE_PERSIST);
  id1.set_id(1234);
  spdy_settings.push_back(std::make_pair(id1, 31337));
  http_server_props_manager_->SetSpdySettings(spdy_server_mail, spdy_settings);

  // Run the task.
  loop_.RunAllPending();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_TRUE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));

  spdy::SpdySettings spdy_settings_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, spdy_settings_ret.size());
  spdy::SpdySetting spdy_setting1_ret = spdy_settings_ret.front();
  spdy::SettingsFlagsAndId id1_ret(spdy_setting1_ret.first);
  EXPECT_EQ(1234U, id1_ret.id());
  EXPECT_EQ(spdy::SETTINGS_FLAG_PERSISTED, id1_ret.flags());
  EXPECT_EQ(31337U, spdy_setting1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ExpectPrefsUpdate();
  // Clear http server data.
  http_server_props_manager_->Clear();

  // Run the task.
  loop_.RunAllPending();

  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_FALSE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));

  spdy::SpdySettings spdy_settings1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, spdy_settings1_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache0) {
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache1) {
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache2) {
  http_server_props_manager_->UpdateCacheFromPrefsOnUIConcrete();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

//
// Tests for shutdown when updating prefs.
//
TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs0) {
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdatePrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs1) {
  ExpectPrefsUpdate();
  // Post an update task.
  http_server_props_manager_->ScheduleUpdatePrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs2) {
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdatePrefsFromCacheOnIOConcrete();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

}  // namespace

}  // namespace chrome_browser_net
