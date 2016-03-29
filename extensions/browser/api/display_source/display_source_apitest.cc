// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate_factory.h"
#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

using api::display_source::SinkInfo;
using api::display_source::SinkState;
using api::display_source::SINK_STATE_DISCONNECTED;
using api::display_source::AUTHENTICATION_METHOD_PBC;

namespace {

DisplaySourceSinkInfo CreateSinkInfoPtr(int id,
                                        const std::string& name,
                                        SinkState state) {
  DisplaySourceSinkInfo info;
  info.id = id;
  info.name = name;
  info.state = state;

  return info;
}

}  // namespace

class MockDisplaySourceConnectionDelegate
    : public DisplaySourceConnectionDelegate {
 public:
  const DisplaySourceSinkInfoList& last_found_sinks() const override {
    return sinks_;
  }
  const Connection* connection() const override { return nullptr; }
  void GetAvailableSinks(const SinkInfoListCallback& sinks_callback,
                         const StringCallback& failure_callback) override {
    AddSink(CreateSinkInfoPtr(1, "sink 1", SINK_STATE_DISCONNECTED));
    sinks_callback.Run(sinks_);
  }

  void RequestAuthentication(int sink_id,
                             const AuthInfoCallback& auth_info_callback,
                             const StringCallback& failure_callback) override {
    DisplaySourceAuthInfo info;
    info.method = AUTHENTICATION_METHOD_PBC;
    auth_info_callback.Run(info);
  }

  void Connect(int sink_id,
               const DisplaySourceAuthInfo& auth_info,
               const StringCallback& failure_callback) override {}

  void Disconnect(const StringCallback& failure_callback) override {}

  void StartWatchingAvailableSinks() override {
    AddSink(CreateSinkInfoPtr(2, "sink 2", SINK_STATE_DISCONNECTED));
  }

  void StopWatchingAvailableSinks() override {}

 private:
  void AddSink(DisplaySourceSinkInfo sink) {
    sinks_.push_back(std::move(sink));
    FOR_EACH_OBSERVER(DisplaySourceConnectionDelegate::Observer, observers_,
                      OnSinksUpdated(sinks_));
  }

  DisplaySourceSinkInfoList sinks_;
};

class DisplaySourceApiTest : public ShellApiTest {
 public:
  DisplaySourceApiTest() = default;

 private:
  static scoped_ptr<KeyedService> CreateMockDelegate(
      content::BrowserContext* profile) {
    return make_scoped_ptr<KeyedService>(
        new MockDisplaySourceConnectionDelegate());
  }

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    DisplaySourceConnectionDelegateFactory::GetInstance()->SetTestingFactory(
        browser_context(), &CreateMockDelegate);
    content::RunAllPendingInMessageLoop();
  }
};

IN_PROC_BROWSER_TEST_F(DisplaySourceApiTest, DisplaySourceExtension) {
  ASSERT_TRUE(RunAppTest("api_test/display_source/api")) << message_;
}

}  // namespace extensions
