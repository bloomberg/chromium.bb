// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/devtools_client.h"
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
  // Listener may be NULL.
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url);

  typedef base::Callback<bool(
      const std::string&,
      int,
      internal::InspectorMessageType*,
      internal::InspectorEvent*,
      internal::InspectorCommandResponse*)> ParserFunc;
  DevToolsClientImpl(const SyncWebSocketFactory& factory,
                     const std::string& url,
                     const ParserFunc& parser_func);

  virtual ~DevToolsClientImpl();

  // Overridden from DevToolsClient:
  virtual Status SendCommand(const std::string& method,
                             const base::DictionaryValue& params) OVERRIDE;
  virtual Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) OVERRIDE;
  virtual void AddListener(DevToolsEventListener* listener) OVERRIDE;

 private:
  Status SendCommandInternal(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result);
  virtual void NotifyEventListeners(const std::string& method,
                                    const base::DictionaryValue& params);
  scoped_ptr<SyncWebSocket> socket_;
  GURL url_;
  ParserFunc parser_func_;
  std::list<DevToolsEventListener*> listeners_;
  bool connected_;
  int next_id_;

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

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_CLIENT_IMPL_H_
