// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client_impl.h"
#include "chrome/test/chromedriver/devtools_event_listener.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/status.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSyncWebSocket : public SyncWebSocket {
 public:
  MockSyncWebSocket() : connected_(false), id_(-1) {}
  virtual ~MockSyncWebSocket() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    EXPECT_STREQ("http://url/", url.possibly_invalid_spec().c_str());
    connected_ = true;
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    EXPECT_TRUE(connected_);
    scoped_ptr<base::Value> value(base::JSONReader::Read(message));
    base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(value->GetAsDictionary(&dict));
    if (!dict)
      return false;
    EXPECT_TRUE(dict->GetInteger("id", &id_));
    std::string method;
    EXPECT_TRUE(dict->GetString("method", &method));
    EXPECT_STREQ("method", method.c_str());
    base::DictionaryValue* params = NULL;
    EXPECT_TRUE(dict->GetDictionary("params", &params));
    if (!params)
      return false;
    int param = -1;
    EXPECT_TRUE(params->GetInteger("param", &param));
    EXPECT_EQ(1, param);
    return true;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    base::DictionaryValue response;
    response.SetInteger("id", id_);
    base::DictionaryValue result;
    result.SetInteger("param", 1);
    response.Set("result", result.DeepCopy());
    base::JSONWriter::Write(&response, message);
    return true;
  }

 private:
  bool connected_;
  int id_;
};

template <typename T>
scoped_ptr<SyncWebSocket> CreateMockSyncWebSocket() {
  return scoped_ptr<SyncWebSocket>(new T());
}

}  // namespace

TEST(DevToolsClientImpl, SendCommand) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  ASSERT_EQ(kOk, client.SendCommand("method", params).code());
}

TEST(DevToolsClientImpl, SendCommandAndGetResult) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  params.SetInteger("param", 1);
  scoped_ptr<base::DictionaryValue> result;
  Status status = client.SendCommandAndGetResult("method", params, &result);
  ASSERT_EQ(kOk, status.code());
  std::string json;
  base::JSONWriter::Write(result.get(), &json);
  ASSERT_STREQ("{\"param\":1}", json.c_str());
}

namespace {

class MockSyncWebSocket2 : public SyncWebSocket {
 public:
  MockSyncWebSocket2() {}
  virtual ~MockSyncWebSocket2() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    return false;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    EXPECT_TRUE(false);
    return false;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    EXPECT_TRUE(false);
    return false;
  }
};

}  // namespace

TEST(DevToolsClientImpl, SendCommandConnectFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket2>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class MockSyncWebSocket3 : public SyncWebSocket {
 public:
  MockSyncWebSocket3() {}
  virtual ~MockSyncWebSocket3() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    return false;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    EXPECT_TRUE(false);
    return false;
  }
};

}  // namespace

TEST(DevToolsClientImpl, SendCommandSendFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket3>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class MockSyncWebSocket4 : public SyncWebSocket {
 public:
  MockSyncWebSocket4() {}
  virtual ~MockSyncWebSocket4() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    return true;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    return false;
  }
};

}  // namespace

TEST(DevToolsClientImpl, SendCommandReceiveNextMessageFails) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket4>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

namespace {

class FakeSyncWebSocket : public SyncWebSocket {
 public:
  FakeSyncWebSocket() : connected_(false) {}
  virtual ~FakeSyncWebSocket() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    EXPECT_FALSE(connected_);
    connected_ = true;
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    return true;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    return true;
  }

 private:
  bool connected_;
};

bool ReturnCommand(
    const std::string& message,
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  command_response->id = expected_id;
  command_response->result.reset(new base::DictionaryValue());
  return true;
}

bool ReturnBadResponse(
    const std::string& message,
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  command_response->id = expected_id;
  command_response->result.reset(new base::DictionaryValue());
  return false;
}

bool ReturnCommandBadId(
    const std::string& message,
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  command_response->id = expected_id + 100;
  command_response->result.reset(new base::DictionaryValue());
  return true;
}

bool ReturnCommandError(
    const std::string& message,
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  *type = internal::kCommandResponseMessageType;
  command_response->id = expected_id;
  command_response->error = "err";
  return true;
}

class MockListener : public DevToolsEventListener {
 public:
  MockListener() : called_(false) {}
  virtual ~MockListener() {
    EXPECT_TRUE(called_);
  }

  virtual void OnEvent(const std::string& method,
                       const base::DictionaryValue& params) OVERRIDE {
    called_ = true;
    EXPECT_STREQ("method", method.c_str());
    EXPECT_TRUE(params.HasKey("key"));
  }

 private:
  bool called_;
};

bool ReturnEventThenResponse(
    bool* first,
    const std::string& message,
    int expected_id,
    internal::InspectorMessageType* type,
    internal::InspectorEvent* event,
    internal::InspectorCommandResponse* command_response) {
  if (*first) {
    *type = internal::kEventMessageType;
    event->method = "method";
    base::DictionaryValue params;
    params.SetInteger("key", 1);
    event->params.reset(params.DeepCopy());
  } else {
    *type = internal::kCommandResponseMessageType;
    command_response->id = expected_id;
    base::DictionaryValue params;
    params.SetInteger("key", 2);
    command_response->result.reset(params.DeepCopy());
  }
  *first = false;
  return true;
}

}  // namespace

TEST(DevToolsClientImpl, SendCommandOnlyConnectsOnce) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", base::Bind(
      &ReturnCommand));
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
}

TEST(DevToolsClientImpl, SendCommandBadResponse) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", base::Bind(
      &ReturnBadResponse));
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST(DevToolsClientImpl, SendCommandBadId) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", base::Bind(
      &ReturnCommandBadId));
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST(DevToolsClientImpl, SendCommandResponseError) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  DevToolsClientImpl client(factory, "http://url", base::Bind(
      &ReturnCommandError));
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsError());
}

TEST(DevToolsClientImpl, SendCommandEventBeforeResponse) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<FakeSyncWebSocket>);
  MockListener listener;
  bool first = true;
  DevToolsClientImpl client(factory, "http://url", base::Bind(
      &ReturnEventThenResponse, &first));
  client.AddListener(&listener);
  base::DictionaryValue params;
  scoped_ptr<base::DictionaryValue> result;
  ASSERT_TRUE(client.SendCommandAndGetResult("method", params, &result).IsOk());
  ASSERT_TRUE(result);
  int key;
  ASSERT_TRUE(result->GetInteger("key", &key));
  ASSERT_EQ(2, key);
}

TEST(ParseInspectorMessage, NonJson) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_FALSE(internal::ParseInspectorMessage(
      "hi", 0, &type, &event, &response));
}

TEST(ParseInspectorMessage, NeitherCommandNorEvent) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_FALSE(internal::ParseInspectorMessage(
      "{}", 0, &type, &event, &response));
}

TEST(ParseInspectorMessage, EventNoParams) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\"}", 0, &type, &event, &response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  ASSERT_TRUE(event.params->IsType(base::Value::TYPE_DICTIONARY));
}

TEST(ParseInspectorMessage, EventWithParams) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"method\":\"method\",\"params\":{\"key\":100}}",
      0, &type, &event, &response));
  ASSERT_EQ(internal::kEventMessageType, type);
  ASSERT_STREQ("method", event.method.c_str());
  int key;
  ASSERT_TRUE(event.params->GetInteger("key", &key));
  ASSERT_EQ(100, key);
}

TEST(ParseInspectorMessage, CommandNoErrorOrResult) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_FALSE(internal::ParseInspectorMessage(
      "{\"id\":1}", 0, &type, &event, &response));
}

TEST(ParseInspectorMessage, CommandError) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"id\":1,\"error\":{}}", 0, &type, &event, &response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_TRUE(response.error.length());
  ASSERT_FALSE(response.result);
}

TEST(ParseInspectorMessage, Command) {
  internal::InspectorMessageType type;
  internal::InspectorEvent event;
  internal::InspectorCommandResponse response;
  ASSERT_TRUE(internal::ParseInspectorMessage(
      "{\"id\":1,\"result\":{\"key\":1}}", 0, &type, &event, &response));
  ASSERT_EQ(internal::kCommandResponseMessageType, type);
  ASSERT_EQ(1, response.id);
  ASSERT_FALSE(response.error.length());
  int key;
  ASSERT_TRUE(response.result->GetInteger("key", &key));
  ASSERT_EQ(1, key);
}
