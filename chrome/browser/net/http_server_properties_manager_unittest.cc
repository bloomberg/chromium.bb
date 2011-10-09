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
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;

class TestingHttpServerPropertiesManager : public HttpServerPropertiesManager {
 public:
  explicit TestingHttpServerPropertiesManager(PrefService* pref_service)
      : HttpServerPropertiesManager(pref_service) {
    InitializeOnIOThread();
  }

  virtual ~TestingHttpServerPropertiesManager() {
  }

  // Make this method public for testing.
  using HttpServerPropertiesManager::ScheduleUpdateCacheOnUI;
  using HttpServerPropertiesManager::ScheduleUpdatePrefsOnIO;

  // Post tasks without a delay during tests.
  virtual void PostUpdateTaskOnUI(Task* task) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  }

  // Makes a direct call to UpdateCacheFromPrefsOnIO during tests.
  void UpdateCacheFromPrefsOnIO() {
    StringVector* spdy_servers = new StringVector;
    spdy_servers->push_back("www.google.com:443");
    HttpServerPropertiesManager::UpdateCacheFromPrefsOnIO(spdy_servers, true);
  }

  void UpdateCacheFromPrefsConcrete() {
    HttpServerPropertiesManager::UpdateCacheFromPrefs();
  }

  // Post tasks without a delay during tests.
  virtual void PostUpdateTaskOnIO(Task* task) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  }

  // Makes a direct call to SetSpdyServersInPrefsOnUI during tests.
  void SetSpdyServersInPrefsOnUI() {
    net::HostPortPair spdy_server_mail("mail.google.com", 443);
    std::string spdy_server_m =
      net::HttpServerPropertiesImpl::GetFlattenedSpdyServer(spdy_server_mail);

    scoped_refptr<RefCountedListValue> spdy_server_list =
        new RefCountedListValue();
    spdy_server_list->data.Append(new StringValue(spdy_server_m));

    HttpServerPropertiesManager::SetSpdyServersInPrefsOnUI(spdy_server_list);
  }

  void UpdatePrefsFromCacheConcrete() {
    HttpServerPropertiesManager::UpdatePrefsFromCache();
  }

  MOCK_METHOD0(UpdateCacheFromPrefs, void());
  MOCK_METHOD0(UpdatePrefsFromCache, void());
  MOCK_METHOD2(UpdateCacheFromPrefsOnIO,
               void(StringVector* spdy_servers, bool support_spdy));
  MOCK_METHOD1(SetSpdyServersInPrefsOnUI,
               void(scoped_refptr<RefCountedListValue> spdy_server_list));

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
    http_server_props_manager_.reset(
        new TestingHttpServerPropertiesManager(&pref_service_));
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
    EXPECT_CALL(*http_server_props_manager_, UpdateCacheFromPrefs())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
            &TestingHttpServerPropertiesManager::UpdateCacheFromPrefsConcrete));
  }

  void ExpectPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdatePrefsFromCache())
        .WillOnce(
            Invoke(http_server_props_manager_.get(),
            &TestingHttpServerPropertiesManager::UpdatePrefsFromCacheConcrete));
  }

  MessageLoop loop_;
  TestingPrefService pref_service_;
  scoped_ptr<TestingHttpServerPropertiesManager> http_server_props_manager_;

 private:
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest, SingleUpdateForTwoPrefChanges) {
  ExpectCacheUpdate();

  ListValue* http_server_props = new ListValue;
  http_server_props->Append(new StringValue("www.google.com:443"));
  http_server_props->Append(new StringValue("mail.google.com:443"));
  pref_service_.SetManagedPref(prefs::kSpdyServers, http_server_props);
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
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

TEST_F(HttpServerPropertiesManagerTest, DeleteAll) {
  ExpectPrefsUpdate();

  // Add mail.google.com:443 as a supporting spdy server.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);

  // Run the task.
  loop_.RunAllPending();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // DeleteAll http server data.
  http_server_props_manager_->DeleteAll();

  // Run the task.
  loop_.RunAllPending();

  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

//
// Tests for shutdown when updating cache.
//
TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache0) {
  EXPECT_CALL(*http_server_props_manager_,
              UpdateCacheFromPrefsOnIO(_, _)).Times(0);
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateCacheOnUI();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache1) {
  EXPECT_CALL(*http_server_props_manager_, UpdateCacheFromPrefs()).Times(0);
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
  EXPECT_CALL(*http_server_props_manager_,
              UpdateCacheFromPrefsOnIO(_, _)).Times(0);
  http_server_props_manager_->UpdateCacheFromPrefsConcrete();
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
  EXPECT_CALL(*http_server_props_manager_,
              SetSpdyServersInPrefsOnUI(_)).Times(0);
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdatePrefsOnIO();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnUIThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs1) {
  EXPECT_CALL(*http_server_props_manager_,
              SetSpdyServersInPrefsOnUI(_)).Times(0);
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
  EXPECT_CALL(*http_server_props_manager_,
              SetSpdyServersInPrefsOnUI(_)).Times(0);
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdatePrefsFromCacheConcrete();
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
