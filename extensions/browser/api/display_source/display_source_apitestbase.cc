// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "extensions/browser/api/display_source/display_source_apitestbase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::display_source::SinkInfo;
using api::display_source::SinkState;
using api::display_source::AuthenticationMethod;
using api::display_source::SINK_STATE_DISCONNECTED;
using api::display_source::SINK_STATE_CONNECTING;
using api::display_source::SINK_STATE_CONNECTED;
using api::display_source::AUTHENTICATION_METHOD_PBC;
using api::display_source::AUTHENTICATION_METHOD_PIN;
using content::BrowserThread;

class MockDisplaySourceConnectionDelegate
    : public DisplaySourceConnectionDelegate,
      public DisplaySourceConnectionDelegate::Connection {
 public:
  MockDisplaySourceConnectionDelegate();

  const DisplaySourceSinkInfoList& last_found_sinks() const override;

  const DisplaySourceConnectionDelegate::Connection* connection()
      const override {
    return (active_sink_ != nullptr &&
           (*active_sink_).state == SINK_STATE_CONNECTED)
      ? this : nullptr;
  }

  void GetAvailableSinks(const SinkInfoListCallback& sinks_callback,
                         const StringCallback& failure_callback) override;

  void RequestAuthentication(int sink_id,
                             const AuthInfoCallback& auth_info_callback,
                             const StringCallback& failure_callback) override;

  void Connect(int sink_id,
               const DisplaySourceAuthInfo& auth_info,
               const StringCallback& failure_callback) override;

  void Disconnect(const StringCallback& failure_callback) override;

  void StartWatchingAvailableSinks() override;

  // DisplaySourceConnectionDelegate::Connection overrides
  DisplaySourceSinkInfo GetConnectedSink() const override;

  void StopWatchingAvailableSinks() override;

  std::string GetLocalAddress() const override;

  std::string GetSinkAddress() const override;

  void SendMessage(const std::string& message) const override;

  void SetMessageReceivedCallback(
      const StringCallback& callback) const override;

 private:
  void AddSink(DisplaySourceSinkInfo sink,
               AuthenticationMethod auth_method,
               const std::string& pin_value);

  void OnSinkConnected();

  void NotifySinksUpdated();

  mutable DisplaySourceSinkInfoList sinks_;
  DisplaySourceSinkInfo* active_sink_;
  std::map<int, std::pair<AuthenticationMethod, std::string>> auth_infos_;
};

namespace {

DisplaySourceSinkInfo CreateSinkInfo(int id, const std::string& name) {
  DisplaySourceSinkInfo ptr;
  ptr.id = id;
  ptr.name = name;
  ptr.state = SINK_STATE_DISCONNECTED;

  return ptr;
}

const char kWiFiDisplayStartSessionMessage[] =
  "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\nRequire: org.wfa.wfd1.0\r\n\r\n";

scoped_ptr<KeyedService> CreateMockDelegate(content::BrowserContext* profile) {
  return make_scoped_ptr<KeyedService>(
    new MockDisplaySourceConnectionDelegate());
}

}  // namespace

void InitMockDisplaySourceConnectionDelegate(content::BrowserContext* profile) {
  DisplaySourceConnectionDelegateFactory::GetInstance()->SetTestingFactory(
    profile, &CreateMockDelegate);
}

MockDisplaySourceConnectionDelegate::MockDisplaySourceConnectionDelegate() {
  active_sink_ = nullptr;
  AddSink(CreateSinkInfo(1, "sink 1"), AUTHENTICATION_METHOD_PIN, "1234");
}

const DisplaySourceSinkInfoList&
MockDisplaySourceConnectionDelegate::last_found_sinks() const {
  return sinks_;
}

void MockDisplaySourceConnectionDelegate::GetAvailableSinks(
    const SinkInfoListCallback& sinks_callback,
    const StringCallback& failure_callback) {
  sinks_callback.Run(sinks_);
}

void MockDisplaySourceConnectionDelegate::RequestAuthentication(
    int sink_id,
    const AuthInfoCallback& auth_info_callback,
    const StringCallback& failure_callback) {
  DisplaySourceAuthInfo info;
  auto it = auth_infos_.find(sink_id);
  ASSERT_NE(it, auth_infos_.end());

  info.method = it->second.first;
  auth_info_callback.Run(info);
}

void MockDisplaySourceConnectionDelegate::Connect(
    int sink_id,
    const DisplaySourceAuthInfo& auth_info,
    const StringCallback& failure_callback) {
  auto it = auth_infos_.find(sink_id);
  ASSERT_NE(it, auth_infos_.end());
  ASSERT_EQ(it->second.first, auth_info.method);
  ASSERT_STREQ(it->second.second.c_str(), auth_info.data->c_str());

  auto found = std::find_if(
    sinks_.begin(), sinks_.end(),
      [sink_id](const DisplaySourceSinkInfo& sink) {
        return sink.id == sink_id;
      });

  ASSERT_NE(found, sinks_.end());
  active_sink_ = sinks_.data() + (found - sinks_.begin());
  active_sink_->state = SINK_STATE_CONNECTING;
  NotifySinksUpdated();

  BrowserThread::PostTask(
    BrowserThread::UI, FROM_HERE,
      base::Bind(&MockDisplaySourceConnectionDelegate::OnSinkConnected,
                 base::Unretained(this)));
}

void MockDisplaySourceConnectionDelegate::Disconnect(
    const StringCallback& failure_callback) {
  ASSERT_TRUE(active_sink_ != nullptr);
  ASSERT_EQ(active_sink_->state, SINK_STATE_CONNECTED);
  active_sink_->state = SINK_STATE_DISCONNECTED;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::StartWatchingAvailableSinks() {
  AddSink(CreateSinkInfo(2, "sink 2"), AUTHENTICATION_METHOD_PBC, "");
}

DisplaySourceSinkInfo MockDisplaySourceConnectionDelegate::GetConnectedSink()
    const {
  EXPECT_TRUE(active_sink_ != nullptr);

  DisplaySourceSinkInfo info;
  info.id = active_sink_->id;
  info.name = active_sink_->name;
  info.state = active_sink_->state;

  return info;
}

void MockDisplaySourceConnectionDelegate::StopWatchingAvailableSinks() {}

std::string MockDisplaySourceConnectionDelegate::GetLocalAddress() const {
  return "127.0.0.1";
}

std::string MockDisplaySourceConnectionDelegate::GetSinkAddress() const {
  return "127.0.0.1";
}

void MockDisplaySourceConnectionDelegate::SendMessage(
    const std::string& message) const {
  ASSERT_STREQ(kWiFiDisplayStartSessionMessage, message.c_str());
}

void MockDisplaySourceConnectionDelegate::SetMessageReceivedCallback(
    const StringCallback& callback) const {}

void MockDisplaySourceConnectionDelegate::AddSink(
    DisplaySourceSinkInfo sink,
    AuthenticationMethod auth_method,
    const std::string& pin_value) {
  auth_infos_[sink.id] = {auth_method, pin_value};
  sinks_.push_back(std::move(sink));
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::OnSinkConnected() {
  active_sink_->state = SINK_STATE_CONNECTED;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::NotifySinksUpdated() {
  FOR_EACH_OBSERVER(DisplaySourceConnectionDelegate::Observer, observers_,
                    OnSinksUpdated(sinks_));
}

}  // namespace extensions
