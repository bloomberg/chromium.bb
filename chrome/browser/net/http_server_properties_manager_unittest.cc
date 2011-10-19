// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/http_server_properties_manager.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

class TestingHttpServerPropertiesManager : public HttpServerPropertiesManager {
 public:
  explicit TestingHttpServerPropertiesManager(PrefService* pref_service)
      : HttpServerPropertiesManager(pref_service) {
    InitializeOnIOThread();
  }

  virtual ~TestingHttpServerPropertiesManager() {
  }

  // Make these method public for testing.
  using HttpServerPropertiesManager::ScheduleUpdateSpdyCacheOnUI;
  using HttpServerPropertiesManager::ScheduleUpdateSpdyPrefsOnIO;
  using HttpServerPropertiesManager::ScheduleUpdateAlternateProtocolCacheOnUI;
  using HttpServerPropertiesManager::ScheduleUpdateAlternateProtocolPrefsOnIO;

  // Post tasks without a delay during tests.
  virtual void StartSpdyPrefsUpdateTimerOnIO(base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartSpdyPrefsUpdateTimerOnIO(
        base::TimeDelta());
  }

  // Post tasks without a delay during tests.
  virtual void StartAlternateProtocolPrefsUpdateTimerOnIO(
      base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartAlternateProtocolPrefsUpdateTimerOnIO(
        base::TimeDelta());
  }

  void UpdateSpdyCacheFromPrefsConcrete() {
    HttpServerPropertiesManager::UpdateSpdyCacheFromPrefs();
  }

  void UpdateAlternateProtocolCacheFromPrefsConcrete() {
    HttpServerPropertiesManager::UpdateAlternateProtocolCacheFromPrefs();
  }

  // Post tasks without a delay during tests.
  virtual void StartSpdyCacheUpdateTimerOnUI(base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartSpdyCacheUpdateTimerOnUI(
        base::TimeDelta());
  }

  // Post tasks without a delay during tests.
  virtual void StartAlternateProtocolCacheUpdateTimerOnUI(
      base::TimeDelta delay) OVERRIDE {
    HttpServerPropertiesManager::StartAlternateProtocolCacheUpdateTimerOnUI(
        base::TimeDelta());
  }

  void UpdateSpdyPrefsFromCacheConcrete() {
    HttpServerPropertiesManager::UpdateSpdyPrefsFromCache();
  }

  void UpdateAlternateProtocolPrefsFromCacheConcrete() {
    HttpServerPropertiesManager::UpdateAlternateProtocolPrefsFromCache();
  }

  MOCK_METHOD0(UpdateSpdyCacheFromPrefs, void());
  MOCK_METHOD0(UpdateSpdyPrefsFromCache, void());
  MOCK_METHOD2(UpdateSpdyCacheFromPrefsOnIO,
               void(std::vector<std::string>* spdy_servers, bool support_spdy));
  MOCK_METHOD1(SetSpdyServersInPrefsOnUI,
               void(scoped_refptr<RefCountedListValue> spdy_server_list));

  MOCK_METHOD0(UpdateAlternateProtocolCacheFromPrefs, void());
  MOCK_METHOD0(UpdateAlternateProtocolPrefsFromCache, void());
  MOCK_METHOD1(UpdateAlternateProtocolCacheFromPrefsOnIO,
               void(RefCountedAlternateProtocolMap*));
  MOCK_METHOD1(SetAlternateProtocolServersInPrefsOnUI,
               void(RefCountedAlternateProtocolMap*));

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
    pref_service_.RegisterListPref(prefs::kSpdyServers);
    pref_service_.RegisterDictionaryPref(prefs::kAlternateProtocolServers);
    http_server_props_manager_.reset(
        new StrictMock<TestingHttpServerPropertiesManager>(&pref_service_));
    ExpectSpdyCacheUpdate();
    ExpectAlternateProtocolCacheUpdate();
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

  void ExpectSpdyCacheUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdateSpdyCacheFromPrefs())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdateSpdyCacheFromPrefsConcrete));
  }

  void ExpectSpdyPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdateSpdyPrefsFromCache())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdateSpdyPrefsFromCacheConcrete));
  }

  void ExpectAlternateProtocolCacheUpdate() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdateAlternateProtocolCacheFromPrefs())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdateAlternateProtocolCacheFromPrefsConcrete));
  }

  void ExpectAlternateProtocolPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdateAlternateProtocolPrefsFromCache())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                   UpdateAlternateProtocolPrefsFromCacheConcrete));
  }

  MessageLoop loop_;
  TestingPrefService pref_service_;
  scoped_ptr<TestingHttpServerPropertiesManager> http_server_props_manager_;

 private:
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest, SingleUpdateForTwoSpdyPrefChanges) {
  ExpectSpdyCacheUpdate();

  // Set up the pref and then set it twice. Only expect a single cache update.
  base::ListValue* http_server_props = new base::ListValue;
  http_server_props->Append(new base::StringValue("www.google.com:443"));
  pref_service_.SetManagedPref(prefs::kSpdyServers, http_server_props);
  http_server_props = http_server_props->DeepCopy();
  http_server_props->Append(new base::StringValue("mail.google.com:443"));
  pref_service_.SetManagedPref(prefs::kSpdyServers, http_server_props);
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("www.google.com:443")));
  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("mail.google.com:443")));
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("foo.google.com:1337")));
}

TEST_F(HttpServerPropertiesManagerTest, SupportsSpdy) {
  ExpectSpdyPrefsUpdate();

  // Post an update task to the IO thread. SetSupportsSpdy calls
  // ScheduleUpdateSpdyPrefsOnIO.

  // Add mail.google.com:443 as a supporting spdy server.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);

  // Run the task.
  loop_.RunAllPending();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, Clear) {
  ExpectSpdyPrefsUpdate();
  ExpectAlternateProtocolPrefsUpdate();

  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);
  http_server_props_manager_->SetAlternateProtocol(
      spdy_server_mail, 443, net::NPN_SPDY_2);

  // Run the task.
  loop_.RunAllPending();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_TRUE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ExpectSpdyPrefsUpdate();
  ExpectAlternateProtocolPrefsUpdate();
  // Clear http server data.
  http_server_props_manager_->Clear();

  // Run the task.
  loop_.RunAllPending();

  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_FALSE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyCache0) {
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateSpdyCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyCache1) {
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateSpdyCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyCache2) {
  http_server_props_manager_->UpdateSpdyCacheFromPrefsConcrete();
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
TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyPrefs0) {
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdateSpdyPrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyPrefs1) {
  ExpectSpdyPrefsUpdate();
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateSpdyPrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateSpdyPrefs2) {
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdateSpdyPrefsFromCacheConcrete();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest,
       SingleUpdateForTwoAlternateProtocolPrefChanges) {
  ExpectAlternateProtocolCacheUpdate();

  // Set up the pref and then set it twice. Only expect a single cache update.
  base::DictionaryValue* alternate_protocol_servers = new base::DictionaryValue;
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 443);
  alternate_protocol->SetInteger("protocol", static_cast<int>(net::NPN_SPDY_1));
  alternate_protocol_servers->SetWithoutPathExpansion(
      "www.google.com:80", alternate_protocol);
  alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 444);
  alternate_protocol->SetInteger("protocol", static_cast<int>(net::NPN_SPDY_2));
  alternate_protocol_servers->SetWithoutPathExpansion(
      "mail.google.com:80", alternate_protocol);
  pref_service_.SetManagedPref(prefs::kAlternateProtocolServers,
                               alternate_protocol_servers);
  alternate_protocol_servers = alternate_protocol_servers->DeepCopy();
  pref_service_.SetManagedPref(prefs::kAlternateProtocolServers,
                               alternate_protocol_servers);
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

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

TEST_F(HttpServerPropertiesManagerTest, HasAlternateProtocol) {
  ExpectAlternateProtocolPrefsUpdate();

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

TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolCache0) {
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateAlternateProtocolCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolCache1) {
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateAlternateProtocolCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolCache2) {
  http_server_props_manager_->UpdateAlternateProtocolCacheFromPrefsConcrete();
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
TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolPrefs0) {
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdateAlternateProtocolPrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolPrefs1) {
  ExpectAlternateProtocolPrefsUpdate();
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateAlternateProtocolPrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest,
       ShutdownWithPendingUpdateAlternateProtocolPrefs2) {
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdateAlternateProtocolPrefsFromCacheConcrete();
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
