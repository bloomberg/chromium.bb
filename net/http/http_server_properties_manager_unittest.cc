// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_manager.h"

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using base::StringPrintf;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

const char kTestHttpServerProperties[] = "TestHttpServerProperties";

class TestingHttpServerPropertiesManager : public HttpServerPropertiesManager {
 public:
  TestingHttpServerPropertiesManager(
      PrefService* pref_service,
      const char* pref_path,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : HttpServerPropertiesManager(pref_service, pref_path, io_task_runner) {
    InitializeOnNetworkThread();
  }

  virtual ~TestingHttpServerPropertiesManager() {}

  // Make these methods public for testing.
  using HttpServerPropertiesManager::ScheduleUpdateCacheOnPrefThread;
  using HttpServerPropertiesManager::ScheduleUpdatePrefsOnNetworkThread;

  // Post tasks without a delay during tests.
  virtual void StartPrefsUpdateTimerOnNetworkThread(
      base::TimeDelta delay) override {
    HttpServerPropertiesManager::StartPrefsUpdateTimerOnNetworkThread(
        base::TimeDelta());
  }

  void UpdateCacheFromPrefsOnUIConcrete() {
    HttpServerPropertiesManager::UpdateCacheFromPrefsOnPrefThread();
  }

  // Post tasks without a delay during tests.
  virtual void StartCacheUpdateTimerOnPrefThread(
      base::TimeDelta delay) override {
    HttpServerPropertiesManager::StartCacheUpdateTimerOnPrefThread(
        base::TimeDelta());
  }

  void UpdatePrefsFromCacheOnNetworkThreadConcrete(
      const base::Closure& callback) {
    HttpServerPropertiesManager::UpdatePrefsFromCacheOnNetworkThread(callback);
  }

  MOCK_METHOD0(UpdateCacheFromPrefsOnPrefThread, void());
  MOCK_METHOD1(UpdatePrefsFromCacheOnNetworkThread, void(const base::Closure&));
  MOCK_METHOD6(UpdateCacheFromPrefsOnNetworkThread,
               void(std::vector<std::string>* spdy_servers,
                    SpdySettingsMap* spdy_settings_map,
                    AlternateProtocolMap* alternate_protocol_map,
                    IPAddressNumber* last_quic_address,
                    ServerNetworkStatsMap* server_network_stats_map,
                    bool detected_corrupted_prefs));
  MOCK_METHOD5(UpdatePrefsOnPref,
               void(base::ListValue* spdy_server_list,
                    SpdySettingsMap* spdy_settings_map,
                    AlternateProtocolMap* alternate_protocol_map,
                    IPAddressNumber* last_quic_address,
                    ServerNetworkStatsMap* server_network_stats_map));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingHttpServerPropertiesManager);
};

class HttpServerPropertiesManagerTest : public testing::Test {
 protected:
  HttpServerPropertiesManagerTest() {}

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kTestHttpServerProperties);
    http_server_props_manager_.reset(
        new StrictMock<TestingHttpServerPropertiesManager>(
            &pref_service_,
            kTestHttpServerProperties,
            base::MessageLoop::current()->message_loop_proxy()));
    ExpectCacheUpdate();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    if (http_server_props_manager_.get())
      http_server_props_manager_->ShutdownOnPrefThread();
    base::RunLoop().RunUntilIdle();
    http_server_props_manager_.reset();
  }

  void ExpectCacheUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdateCacheFromPrefsOnPrefThread())
        .WillOnce(Invoke(http_server_props_manager_.get(),
                         &TestingHttpServerPropertiesManager::
                             UpdateCacheFromPrefsOnUIConcrete));
  }

  void ExpectPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdatePrefsFromCacheOnNetworkThread(_))
        .WillOnce(Invoke(http_server_props_manager_.get(),
                         &TestingHttpServerPropertiesManager::
                             UpdatePrefsFromCacheOnNetworkThreadConcrete));
  }

  void ExpectPrefsUpdateRepeatedly() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdatePrefsFromCacheOnNetworkThread(_))
        .WillRepeatedly(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                       UpdatePrefsFromCacheOnNetworkThreadConcrete));
  }

  bool HasAlternateProtocol(const HostPortPair& server) {
    const AlternateProtocolInfo alternate =
        http_server_props_manager_->GetAlternateProtocol(server);
    return alternate.protocol != UNINITIALIZED_ALTERNATE_PROTOCOL;
  }

  //base::RunLoop loop_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<TestingHttpServerPropertiesManager> http_server_props_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest,
       SingleUpdateForTwoSpdyServerPrefChanges) {
  ExpectCacheUpdate();

  // Set up the prefs for www.google.com:80 and mail.google.com:80 and then set
  // it twice. Only expect a single cache update.

  base::DictionaryValue* server_pref_dict = new base::DictionaryValue;
  HostPortPair google_server("www.google.com", 80);
  HostPortPair mail_server("mail.google.com", 80);

  // Set supports_spdy for www.google.com:80.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for www.google.com:80.
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 443);
  alternate_protocol->SetString("protocol_str", "npn-spdy/3");
  server_pref_dict->SetWithoutPathExpansion("alternate_protocol",
                                            alternate_protocol);

  // Set up ServerNetworkStats for www.google.com:80.
  base::DictionaryValue* stats = new base::DictionaryValue;
  stats->SetInteger("srtt", 10);
  server_pref_dict->SetWithoutPathExpansion("network_stats", stats);

  // Set the server preference for www.google.com:80.
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  servers_dict->SetWithoutPathExpansion("www.google.com:80", server_pref_dict);

  // Set the preference for mail.google.com server.
  base::DictionaryValue* server_pref_dict1 = new base::DictionaryValue;

  // Set supports_spdy for mail.google.com:80
  server_pref_dict1->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for mail.google.com:80
  base::DictionaryValue* alternate_protocol1 = new base::DictionaryValue;
  alternate_protocol1->SetInteger("port", 444);
  alternate_protocol1->SetString("protocol_str", "npn-spdy/3.1");

  server_pref_dict1->SetWithoutPathExpansion("alternate_protocol",
                                             alternate_protocol1);

  // Set up ServerNetworkStats for mail.google.com:80.
  base::DictionaryValue* stats1 = new base::DictionaryValue;
  stats1->SetInteger("srtt", 20);
  server_pref_dict1->SetWithoutPathExpansion("network_stats", stats1);
  // Set the server preference for mail.google.com:80.
  servers_dict->SetWithoutPathExpansion("mail.google.com:80",
                                        server_pref_dict1);

  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  HttpServerPropertiesManager::SetVersion(http_server_properties_dict, -1);
  http_server_properties_dict->SetWithoutPathExpansion("servers", servers_dict);
  base::DictionaryValue* supports_quic = new base::DictionaryValue;
  supports_quic->SetBoolean("used_quic", true);
  supports_quic->SetString("address", "127.0.0.1");
  http_server_properties_dict->SetWithoutPathExpansion("supports_quic",
                                                       supports_quic);

  // Set the same value for kHttpServerProperties multiple times.
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict);
  base::DictionaryValue* http_server_properties_dict2 =
      http_server_properties_dict->DeepCopy();
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict2);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify SupportsSpdy.
  EXPECT_TRUE(
      http_server_props_manager_->SupportsRequestPriority(google_server));
  EXPECT_TRUE(http_server_props_manager_->SupportsRequestPriority(mail_server));
  EXPECT_FALSE(http_server_props_manager_->SupportsRequestPriority(
      HostPortPair::FromString("foo.google.com:1337")));

  // Verify AlternateProtocol.
  AlternateProtocolInfo port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(google_server);
  EXPECT_EQ(443, port_alternate_protocol.port);
  EXPECT_EQ(NPN_SPDY_3, port_alternate_protocol.protocol);
  port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(mail_server);
  EXPECT_EQ(444, port_alternate_protocol.port);
  EXPECT_EQ(NPN_SPDY_3_1, port_alternate_protocol.protocol);

  // Verify SupportsQuic.
  IPAddressNumber last_address;
  EXPECT_TRUE(http_server_props_manager_->GetSupportsQuic(&last_address));
  EXPECT_EQ("127.0.0.1", IPAddressToString(last_address));

  // Verify ServerNetworkStats.
  const ServerNetworkStats* stats2 =
      http_server_props_manager_->GetServerNetworkStats(google_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  const ServerNetworkStats* stats3 =
      http_server_props_manager_->GetServerNetworkStats(mail_server);
  EXPECT_EQ(20, stats3->srtt.ToInternalValue());
}

TEST_F(HttpServerPropertiesManagerTest, BadCachedHostPortPair) {
  ExpectCacheUpdate();
  // The prefs are automaticalls updated in the case corruption is detected.
  ExpectPrefsUpdate();

  base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

  // Set supports_spdy for www.google.com:65536.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for www.google.com:65536.
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 80);
  alternate_protocol->SetString("protocol_str", "npn-spdy/3");
  server_pref_dict->SetWithoutPathExpansion("alternate_protocol",
                                            alternate_protocol);

  // Set up ServerNetworkStats for www.google.com:65536.
  base::DictionaryValue* stats = new base::DictionaryValue;
  stats->SetInteger("srtt", 10);
  server_pref_dict->SetWithoutPathExpansion("network_stats", stats);

  // Set the server preference for www.google.com:65536.
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  servers_dict->SetWithoutPathExpansion("www.google.com:65536",
                                        server_pref_dict);

  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  HttpServerPropertiesManager::SetVersion(http_server_properties_dict, -1);
  http_server_properties_dict->SetWithoutPathExpansion("servers", servers_dict);

  // Set up the pref.
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify that nothing is set.
  EXPECT_FALSE(http_server_props_manager_->SupportsRequestPriority(
      HostPortPair::FromString("www.google.com:65536")));
  EXPECT_FALSE(
      HasAlternateProtocol(HostPortPair::FromString("www.google.com:65536")));
  const ServerNetworkStats* stats1 =
      http_server_props_manager_->GetServerNetworkStats(
          HostPortPair::FromString("www.google.com:65536"));
  EXPECT_EQ(NULL, stats1);
}

TEST_F(HttpServerPropertiesManagerTest, BadCachedAltProtocolPort) {
  ExpectCacheUpdate();
  // The prefs are automaticalls updated in the case corruption is detected.
  ExpectPrefsUpdate();

  base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

  // Set supports_spdy for www.google.com:80.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for www.google.com:80.
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 65536);
  alternate_protocol->SetString("protocol_str", "npn-spdy/3");
  server_pref_dict->SetWithoutPathExpansion("alternate_protocol",
                                            alternate_protocol);

  // Set the server preference for www.google.com:80.
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  servers_dict->SetWithoutPathExpansion("www.google.com:80", server_pref_dict);

  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  HttpServerPropertiesManager::SetVersion(http_server_properties_dict, -1);
  http_server_properties_dict->SetWithoutPathExpansion("servers", servers_dict);

  // Set up the pref.
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify AlternateProtocol is not set.
  EXPECT_FALSE(
      HasAlternateProtocol(HostPortPair::FromString("www.google.com:80")));
}

TEST_F(HttpServerPropertiesManagerTest, SupportsSpdy) {
  ExpectPrefsUpdate();

  // Post an update task to the network thread. SetSupportsSpdy calls
  // ScheduleUpdatePrefsOnNetworkThread.

  // Add mail.google.com:443 as a supporting spdy server.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(
      http_server_props_manager_->SupportsRequestPriority(spdy_server_mail));
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      http_server_props_manager_->SupportsRequestPriority(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, SetSpdySetting) {
  ExpectPrefsUpdate();

  // Add SpdySetting for mail.google.com:443.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ClearSpdySetting) {
  ExpectPrefsUpdateRepeatedly();

  // Add SpdySetting for mail.google.com:443.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear SpdySetting for mail.google.com:443.
  http_server_props_manager_->ClearSpdySettings(spdy_server_mail);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  // Verify that there are no entries in the settings map for
  // mail.google.com:443.
  const SettingsMap& settings_map2_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(0U, settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ClearAllSpdySetting) {
  ExpectPrefsUpdateRepeatedly();

  // Add SpdySetting for mail.google.com:443.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear All SpdySettings.
  http_server_props_manager_->ClearAllSpdySettings();

  // Run the task.
  base::RunLoop().RunUntilIdle();

  // Verify that there are no entries in the settings map.
  const SpdySettingsMap& spdy_settings_map2_ret =
      http_server_props_manager_->spdy_settings_map();
  ASSERT_EQ(0U, spdy_settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, GetAlternateProtocol) {
  ExpectPrefsUpdate();

  HostPortPair spdy_server_mail("mail.google.com", 80);
  EXPECT_FALSE(HasAlternateProtocol(spdy_server_mail));
  http_server_props_manager_->SetAlternateProtocol(spdy_server_mail, 443,
                                                   NPN_SPDY_3, 1.0);

  // Run the task.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  const AlternateProtocolInfo alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(spdy_server_mail);
  EXPECT_EQ(443, alternate_protocol.port);
  EXPECT_EQ(NPN_SPDY_3, alternate_protocol.protocol);
  EXPECT_EQ(1.0, alternate_protocol.probability);
}

TEST_F(HttpServerPropertiesManagerTest, SupportsQuic) {
  ExpectPrefsUpdate();

  IPAddressNumber address;
  EXPECT_FALSE(http_server_props_manager_->GetSupportsQuic(&address));

  IPAddressNumber actual_address;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &actual_address));
  http_server_props_manager_->SetSupportsQuic(true, actual_address);

  // Run the task.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  EXPECT_TRUE(http_server_props_manager_->GetSupportsQuic(&address));
  EXPECT_EQ(actual_address, address);
}

TEST_F(HttpServerPropertiesManagerTest, ServerNetworkStats) {
  ExpectPrefsUpdate();

  HostPortPair mail_server("mail.google.com", 80);
  const ServerNetworkStats* stats =
      http_server_props_manager_->GetServerNetworkStats(mail_server);
  EXPECT_EQ(NULL, stats);
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  http_server_props_manager_->SetServerNetworkStats(mail_server, stats1);

  // Run the task.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  const ServerNetworkStats* stats2 =
      http_server_props_manager_->GetServerNetworkStats(mail_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
}

TEST_F(HttpServerPropertiesManagerTest, Clear) {
  ExpectPrefsUpdate();

  HostPortPair spdy_server_mail("mail.google.com", 443);
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);
  http_server_props_manager_->SetAlternateProtocol(spdy_server_mail, 443,
                                                   NPN_SPDY_3, 1.0);
  IPAddressNumber actual_address;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &actual_address));
  http_server_props_manager_->SetSupportsQuic(true, actual_address);
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromMicroseconds(10);
  http_server_props_manager_->SetServerNetworkStats(spdy_server_mail, stats);

  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      http_server_props_manager_->SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(HasAlternateProtocol(spdy_server_mail));
  IPAddressNumber address;
  EXPECT_TRUE(http_server_props_manager_->GetSupportsQuic(&address));
  EXPECT_EQ(actual_address, address);
  const ServerNetworkStats* stats1 =
      http_server_props_manager_->GetServerNetworkStats(spdy_server_mail);
  EXPECT_EQ(10, stats1->srtt.ToInternalValue());

  // Check SPDY settings values.
  const SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ExpectPrefsUpdate();

  // Clear http server data, time out if we do not get a completion callback.
  http_server_props_manager_->Clear(base::MessageLoop::QuitClosure());
  base::RunLoop().Run();

  EXPECT_FALSE(
      http_server_props_manager_->SupportsRequestPriority(spdy_server_mail));
  EXPECT_FALSE(HasAlternateProtocol(spdy_server_mail));
  EXPECT_FALSE(http_server_props_manager_->GetSupportsQuic(&address));
  const ServerNetworkStats* stats2 =
      http_server_props_manager_->GetServerNetworkStats(spdy_server_mail);
  EXPECT_EQ(NULL, stats2);

  const SettingsMap& settings_map2_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

// https://crbug.com/444956: Add 200 alternate_protocol servers followed by
// supports_quic and verify we have read supports_quic from prefs.
TEST_F(HttpServerPropertiesManagerTest, BadSupportsQuic) {
  ExpectCacheUpdate();

  base::DictionaryValue* servers_dict = new base::DictionaryValue;

  for (int i = 0; i < 200; ++i) {
    // Set up alternate_protocol for www.google.com:i.
    base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
    alternate_protocol->SetInteger("port", i);
    alternate_protocol->SetString("protocol_str", "npn-spdy/3");
    base::DictionaryValue* server_pref_dict = new base::DictionaryValue;
    server_pref_dict->SetWithoutPathExpansion("alternate_protocol",
                                              alternate_protocol);
    servers_dict->SetWithoutPathExpansion(StringPrintf("www.google.com:%d", i),
                                          server_pref_dict);
  }

  // Set the preference for mail.google.com server.
  base::DictionaryValue* server_pref_dict1 = new base::DictionaryValue;

  // Set the server preference for mail.google.com:80.
  servers_dict->SetWithoutPathExpansion("mail.google.com:80",
                                        server_pref_dict1);

  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  HttpServerPropertiesManager::SetVersion(http_server_properties_dict, -1);
  http_server_properties_dict->SetWithoutPathExpansion("servers", servers_dict);

  // Set up SupportsQuic for 127.0.0.1
  base::DictionaryValue* supports_quic = new base::DictionaryValue;
  supports_quic->SetBoolean("used_quic", true);
  supports_quic->SetString("address", "127.0.0.1");
  http_server_properties_dict->SetWithoutPathExpansion("supports_quic",
                                                       supports_quic);

  // Set up the pref.
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify AlternateProtocol.
  for (int i = 0; i < 200; ++i) {
    std::string server = StringPrintf("www.google.com:%d", i);
    AlternateProtocolInfo port_alternate_protocol =
        http_server_props_manager_->GetAlternateProtocol(
            HostPortPair::FromString(server));
    EXPECT_EQ(i, port_alternate_protocol.port);
    EXPECT_EQ(NPN_SPDY_3, port_alternate_protocol.protocol);
  }

  // Verify SupportsQuic.
  IPAddressNumber address;
  ASSERT_TRUE(http_server_props_manager_->GetSupportsQuic(&address));
  EXPECT_EQ("127.0.0.1", IPAddressToString(address));
}

TEST_F(HttpServerPropertiesManagerTest, UpdateCacheWithPrefs) {
  const HostPortPair server_www("www.google.com", 80);
  const HostPortPair server_mail("mail.google.com", 80);

  // Set alternate protocol.
  http_server_props_manager_->SetAlternateProtocol(server_www, 443, NPN_SPDY_3,
                                                   1.0);
  http_server_props_manager_->SetAlternateProtocol(server_mail, 444,
                                                   NPN_SPDY_3_1, 0.2);

  // Set ServerNetworkStats.
  ServerNetworkStats stats;
  stats.srtt = base::TimeDelta::FromInternalValue(42);
  http_server_props_manager_->SetServerNetworkStats(server_mail, stats);

  // Set SupportsQuic.
  IPAddressNumber actual_address;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &actual_address));
  http_server_props_manager_->SetSupportsQuic(true, actual_address);

  // Update cache.
  ExpectPrefsUpdate();
  ExpectCacheUpdate();
  http_server_props_manager_->ScheduleUpdateCacheOnPrefThread();
  base::RunLoop().RunUntilIdle();

  // Verify preferences.
  const char expected_json[] = "{"
    "\"servers\":{"
      "\"mail.google.com:80\":{"
        "\"alternate_protocol\":{"
          "\"port\":444,\"probability\":0.2,\"protocol_str\":\"npn-spdy/3.1\""
        "},"
        "\"network_stats\":{"
          "\"srtt\":42"
        "}"
      "},"
      "\"www.google.com:80\":{"
        "\"alternate_protocol\":{"
          "\"port\":443,\"probability\":1.0,\"protocol_str\":\"npn-spdy/3\""
        "}"
      "}"
    "},"
    "\"supports_quic\":{"
      "\"address\":\"127.0.0.1\",\"used_quic\":true"
    "},"
    "\"version\":3"
  "}";

  const base::Value* http_server_properties =
      pref_service_.GetUserPref(kTestHttpServerProperties);
  ASSERT_NE(nullptr, http_server_properties);
  std::string preferences_json;
  EXPECT_TRUE(
      base::JSONWriter::Write(http_server_properties, &preferences_json));
  EXPECT_EQ(expected_json, preferences_json);
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache0) {
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateCacheOnPrefThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache1) {
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateCacheOnPrefThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache2) {
  http_server_props_manager_->UpdateCacheFromPrefsOnUIConcrete();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

//
// Tests for shutdown when updating prefs.
//
TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs0) {
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdatePrefsOnNetworkThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs1) {
  ExpectPrefsUpdate();
  // Post an update task.
  http_server_props_manager_->ScheduleUpdatePrefsOnNetworkThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs2) {
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdatePrefsFromCacheOnNetworkThreadConcrete(
      base::Closure());
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace net
