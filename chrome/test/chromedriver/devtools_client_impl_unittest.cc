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

class MockSyncWebSocket5 : public SyncWebSocket {
 public:
  explicit MockSyncWebSocket5(const std::list<std::string> messages)
      : messages_(messages) {}
  virtual ~MockSyncWebSocket5() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    return true;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    if (messages_.empty())
      return false;
    *message = messages_.front();
    messages_.pop_front();
    return true;
  }

 private:
  std::list<std::string> messages_;
};

scoped_ptr<SyncWebSocket> CreateMockSyncWebSocket5(
    const std::list<std::string>& messages) {
  return scoped_ptr<SyncWebSocket>(new MockSyncWebSocket5(messages));
}

}  // namespace

TEST(DevToolsClientImpl, SendCommandBadResponses) {
  std::list<std::string> msgs;
  msgs.push_back("");
  msgs.push_back("{}");
  msgs.push_back("{\"id\":false}");
  msgs.push_back("{\"id\":1000}");
  msgs.push_back("{\"id\":5,\"error\":{\"key\":\"whoops\"}}");
  SyncWebSocketFactory factory = base::Bind(&CreateMockSyncWebSocket5, msgs);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  for (size_t i = 0; i < msgs.size() - 1; ++i) {
    ASSERT_TRUE(client.SendCommand("method", params).IsError());
  }
  Status status = client.SendCommand("method", params);
  ASSERT_TRUE(status.IsError());
  ASSERT_NE(std::string::npos, status.message().find("whoops"));
}

TEST(DevToolsClientImpl, SendCommandAndGetResultBadResults) {
  std::list<std::string> msgs;
  msgs.push_back("{\"id\":1}");
  msgs.push_back("{\"id\":2,\"result\":1}");
  msgs.push_back("{\"id\":3,\"error\":{\"key\":\"whoops\"},\"result\":{}}");
  SyncWebSocketFactory factory = base::Bind(&CreateMockSyncWebSocket5, msgs);
  DevToolsClientImpl client(factory, "http://url");
  scoped_ptr<base::DictionaryValue> result;
  base::DictionaryValue params;
  for (size_t i = 0; i < msgs.size() - 1; ++i) {
    Status status = client.SendCommandAndGetResult("method", params, &result);
    ASSERT_TRUE(status.IsError());
    ASSERT_FALSE(result);
  }
  Status status = client.SendCommand("method", params);
  ASSERT_TRUE(status.IsError());
  ASSERT_NE(std::string::npos, status.message().find("whoops"));
}

namespace {

class MockSyncWebSocket6 : public SyncWebSocket {
 public:
  MockSyncWebSocket6() : connected_(false), id_(-1) {}
  virtual ~MockSyncWebSocket6() {}

  virtual bool Connect(const GURL& url) OVERRIDE {
    EXPECT_FALSE(connected_);
    connected_ = true;
    return true;
  }

  virtual bool Send(const std::string& message) OVERRIDE {
    scoped_ptr<base::Value> value(base::JSONReader::Read(message));
    base::DictionaryValue* dict = NULL;
    EXPECT_TRUE(value->GetAsDictionary(&dict));
    if (!dict)
      return false;
    EXPECT_TRUE(dict->GetInteger("id", &id_));
    return true;
  }

  virtual bool ReceiveNextMessage(std::string* message) OVERRIDE {
    base::DictionaryValue response;
    response.SetInteger("id", id_);
    base::JSONWriter::Write(&response, message);
    return true;
  }

 private:
  bool connected_;
  int id_;
};

}  // namespace

TEST(DevToolsClientImpl, SendCommandOnlyConnectsOnce) {
  SyncWebSocketFactory factory =
      base::Bind(&CreateMockSyncWebSocket<MockSyncWebSocket6>);
  DevToolsClientImpl client(factory, "http://url");
  base::DictionaryValue params;
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
  ASSERT_TRUE(client.SendCommand("method", params).IsOk());
}
