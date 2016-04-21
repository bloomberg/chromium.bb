// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/display_source_apitestbase.h"

#include <map>
#include <utility>

#include "base/memory/ptr_util.h"
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

  DisplaySourceConnectionDelegate::Connection* connection()
      override {
    return (active_sink_ && active_sink_->state == SINK_STATE_CONNECTED)
               ? this
               : nullptr;
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
  const DisplaySourceSinkInfo& GetConnectedSink() const override;

  void StopWatchingAvailableSinks() override;

  std::string GetLocalAddress() const override;

  std::string GetSinkAddress() const override;

  void SendMessage(const std::string& message) override;

  void SetMessageReceivedCallback(
      const StringCallback& callback) override;

 private:
  void AddSink(DisplaySourceSinkInfo sink,
               AuthenticationMethod auth_method,
               const std::string& pin_value);

  void OnSinkConnected();

  void NotifySinksUpdated();

  void EnqueueSinkMessage(std::string message);

  void CheckSourceMessageContent(std::string pattern,
                                 const std::string& message);

  DisplaySourceSinkInfoList sinks_;
  DisplaySourceSinkInfo* active_sink_;
  std::map<int, std::pair<AuthenticationMethod, std::string>> auth_infos_;
  StringCallback message_received_cb_;

  struct Message {
    enum Direction {
      SourceToSink,
      SinkToSource
    };
    std::string data;
    Direction direction;

    bool is_from_sink() const { return direction == SinkToSource; }
    Message(const std::string& message_data, Direction direction)
      : data(message_data), direction(direction) {}
  };

  std::list<Message> messages_list_;
  std::string session_id_;
};

namespace {

const size_t kSessionIdLength = 8;
const char kSessionKey[] = "Session: ";

DisplaySourceSinkInfo CreateSinkInfo(int id, const std::string& name) {
  DisplaySourceSinkInfo ptr;
  ptr.id = id;
  ptr.name = name;
  ptr.state = SINK_STATE_DISCONNECTED;

  return ptr;
}

std::unique_ptr<KeyedService> CreateMockDelegate(
    content::BrowserContext* profile) {
  return base::WrapUnique<KeyedService>(
      new MockDisplaySourceConnectionDelegate());
}

}  // namespace

void InitMockDisplaySourceConnectionDelegate(content::BrowserContext* profile) {
  DisplaySourceConnectionDelegateFactory::GetInstance()->SetTestingFactory(
    profile, &CreateMockDelegate);
}
namespace {

// WiFi Display session RTSP messages patterns.

const char kM1Message[] = "OPTIONS * RTSP/1.0\r\n"
                          "CSeq: 1\r\n"
                          "Require: org.wfa.wfd1.0\r\n\r\n";

const char kM1MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq:1\r\n"
                               "Public: org.wfa.wfd1.0, "
                               "GET_PARAMETER, SET_PARAMETER\r\n\r\n";

const char kM2Message[] = "OPTIONS * RTSP/1.0\r\n"
                          "CSeq: 2\r\n"
                          "Require: org.wfa.wfd1.0\r\n\r\n";

const char kM2MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 2\r\n"
                               "Public: org.wfa.wfd1.0, "
                               "GET_PARAMETER, SET_PARAMETER, PLAY, PAUSE, "
                               "SETUP, TEARDOWN\r\n\r\n";

const char kM3Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 2\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 41\r\n\r\n"
                          "wfd_video_formats\r\n"
                          "wfd_client_rtp_ports\r\n";

const char kM3MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 2\r\n"
                               "Content-Type: text/parameters\r\n"
                               "Content-Length: 145\r\n\r\n"
                               "wfd_video_formats: "
                               "40 00 02 10 0001FFFF 1FFFFFFF 00000FFF 00 0000 "
                               "0000 00 none none\r\n"
                               "wfd_client_rtp_ports: RTP/AVP/UDP;"
                               "unicast 41657 0 mode=play\r\n";

const char kM4Message[] = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 3\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 209\r\n\r\n"
                          "wfd_client_rtp_ports: "
                          "RTP/AVP/UDP;unicast 41657 0 mode=play\r\n"
                          "wfd_presentation_URL: "
                          "rtsp://127.0.0.1/wfd1.0/streamid=0 none\r\n"
                          "wfd_video_formats: "
                          "00 00 02 10 00000001 00000000 00000000 00 0000 0000 "
                          "00 none none\r\n";

const char kM4MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 3\r\n\r\n";

const char kM5Message[] = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 4\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 27\r\n\r\n"
                          "wfd_trigger_method: SETUP\r\n";

const char kM5MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 4\r\n\r\n";

const char kM6Message[] = "SETUP rtsp://localhost/wfd1.0/streamid=0 "
                          "RTSP/1.0\r\n"
                          "CSeq: 3\r\n"
                          "Transport: RTP/AVP/UDP;unicast;"
                          "client_port=41657\r\n\r\n";

const char kM6MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 3\r\n"
                               "Session: 00000000;timeout=60\r\n"
                               "Transport: RTP/AVP/UDP;unicast;"
                               "client_port=41657\r\n\r\n";

const char kM7Message[] = "PLAY rtsp://localhost/wfd1.0/streamid=0 RTSP/1.0\r\n"
                          "CSeq: 4\r\n"
                          "Session: 00000000\r\n\r\n";

const char kM7MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 4\r\n\r\n";

const char kM8Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 5\r\n\r\n";
} // namespace
MockDisplaySourceConnectionDelegate::MockDisplaySourceConnectionDelegate()
    : active_sink_(nullptr) {
  messages_list_.push_back(Message(kM1Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM1MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM2Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM2MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM3Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM3MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM4Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM4MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM5Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM5MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM6Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM6MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM7Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM7MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM8Message, Message::SourceToSink));

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
  CHECK(active_sink_);
  ASSERT_EQ(active_sink_->state, SINK_STATE_CONNECTED);
  active_sink_->state = SINK_STATE_DISCONNECTED;
  active_sink_ = nullptr;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::StartWatchingAvailableSinks() {
  AddSink(CreateSinkInfo(2, "sink 2"), AUTHENTICATION_METHOD_PBC, "");
}

const DisplaySourceSinkInfo&
MockDisplaySourceConnectionDelegate::GetConnectedSink() const {
  CHECK(active_sink_);
  return *active_sink_;
}

void MockDisplaySourceConnectionDelegate::StopWatchingAvailableSinks() {}

std::string MockDisplaySourceConnectionDelegate::GetLocalAddress() const {
  return "127.0.0.1";
}

std::string MockDisplaySourceConnectionDelegate::GetSinkAddress() const {
  return "127.0.0.1";
}

void MockDisplaySourceConnectionDelegate::SendMessage(
    const std::string& message) {
  ASSERT_FALSE(messages_list_.empty());
  ASSERT_FALSE(messages_list_.front().is_from_sink());

  CheckSourceMessageContent(messages_list_.front().data, message);
  messages_list_.pop_front();

  while (!messages_list_.empty() && messages_list_.front().is_from_sink()) {
    EnqueueSinkMessage(messages_list_.front().data);
    messages_list_.pop_front();
  }
}

void MockDisplaySourceConnectionDelegate::SetMessageReceivedCallback(
    const StringCallback& callback) {
  message_received_cb_ = callback;
}

void MockDisplaySourceConnectionDelegate::AddSink(
    DisplaySourceSinkInfo sink,
    AuthenticationMethod auth_method,
    const std::string& pin_value) {
  auth_infos_[sink.id] = {auth_method, pin_value};
  sinks_.push_back(std::move(sink));
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::OnSinkConnected() {
  CHECK(active_sink_);
  active_sink_->state = SINK_STATE_CONNECTED;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::NotifySinksUpdated() {
  FOR_EACH_OBSERVER(DisplaySourceConnectionDelegate::Observer, observers_,
                    OnSinksUpdated(sinks_));
}

void MockDisplaySourceConnectionDelegate::
EnqueueSinkMessage(std::string message) {
  const std::size_t found = message.find(kSessionKey);
  if (found != std::string::npos) {
    const std::size_t session_id_pos = found +
        std::char_traits<char>::length(kSessionKey);
    message.replace(session_id_pos, kSessionIdLength, session_id_);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(message_received_cb_, message));
}

void MockDisplaySourceConnectionDelegate::
CheckSourceMessageContent(std::string pattern,
                          const std::string& message) {
  // Message M6_reply from Source to Sink has a unique and random session id
  // generated by Source. The id cannot be predicted and the session id should
  // be extracted and added to the message pattern for assertion.
  // The following code checks if messages include "Session" string.
  // If not, assert the message normally.
  // If yes, find the session id, add it to the pattern and to the sink message
  // that has Session: substring inside.
  const std::size_t found = message.find(kSessionKey);

  if (found != std::string::npos) {
    const std::size_t session_id_pos = found +
        std::char_traits<char>::length(kSessionKey);
    session_id_ = message.substr(session_id_pos, kSessionIdLength);

    pattern.replace(session_id_pos, kSessionIdLength, session_id_);
  }
  ASSERT_EQ(pattern, message);
}

}  // namespace extensions
