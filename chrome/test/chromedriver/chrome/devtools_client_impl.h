// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "googleurl/src/gurl.h"

namespace base {
class DictionaryValue;
}

namespace internal {

enum InspectorMessageType {
  kEventMessageType = 0,
  kCommandResponseMessageType
};

struct InspectorEvent {
  InspectorEvent();
  ~InspectorEvent();
  std::string method;
  scoped_ptr<base::DictionaryValue> params;
};

struct InspectorCommandResponse {
  InspectorCommandResponse();
  ~InspectorCommandResponse();
  int id;
  std::string error;
  scoped_ptr<base::DictionaryValue> result;
};

}  // namespace internal

class DevToolsEventListener;
class Status;
class SyncWebSocket;

class DevToolsClientImpl : public DevToolsClient {
 public:
  typedef base::Callback<Status()> FrontendCloserFunc;
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const FrontendCloserFunc& frontend_closer_func);

  typedef base::Callback<bool(
      const std::string&,
      int,
      internal::InspectorMessageType*,
      internal::InspectorEvent*,
      internal::InspectorCommandResponse*)> ParserFunc;
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const FrontendCloserFunc& frontend_closer_func,
                     const ParserFunc& parser_func);

  virtual ~DevToolsClientImpl();

  void SetParserFuncForTesting(const ParserFunc& parser_func);

  // Overridden from DevToolsClient:
  virtual Status ConnectIfNecessary() OVERRIDE;
  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) OVERRIDE;
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE;
  virtual void AddListener(DevToolsEventListener* listener) OVERRIDE;
  virtual Status HandleEventsUntil(
      const ConditionalFunc& conditional_func) OVERRIDE;

 private:
  typedef std::map<int, base::DictionaryValue*> ResponseMap;

  Status SendCommandInternal(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result);
  Status ReceiveNextMessage(int expected_id);
  bool HasReceivedCommandResponse(int cmd_id);
  Status EnsureListenersNotifiedOfConnect();
  Status EnsureListenersNotifiedOfEvent();

  scoped_ptr<SyncWebSocket> socket_;
  GURL url_;
  FrontendCloserFunc frontend_closer_func_;
  ParserFunc parser_func_;
  std::list<DevToolsEventListener*> listeners_;
  std::list<DevToolsEventListener*> unnotified_connect_listeners_;
  std::list<DevToolsEventListener*> unnotified_event_listeners_;
  internal::InspectorEvent* unnotified_event_;
  ResponseMap cmd_response_map_;
  int next_id_;
  int stack_count_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClientImpl);
};

namespace internal {

bool ParseInspectorMessage(
    const std::string& message,
    int expected_id,
    InspectorMessageType* type,
    InspectorEvent* event,
    InspectorCommandResponse* command_response);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_DEVTOOLS_CLIENT_IMPL_H_
