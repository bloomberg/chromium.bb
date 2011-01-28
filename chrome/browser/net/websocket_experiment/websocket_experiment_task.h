// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebSocket live experiment task.
// It will try the following scenario.
//
//  - Fetch |http_url| within |url_fetch_deadline_ms| msec.
//    If failed, the task is aborted (no http reachability)
//
//  - Connect to |url| with WebSocket protocol within
//    |websocket_onopen_deadline_ms| msec.
//    Checks WebSocket connection can be established.
//
//  - Send |websocket_hello_message| on the WebSocket connection and
//    wait it from server within |websocket_hello_echoback_deadline_ms| msec.
//    Checks message can be sent/received on the WebSocket connection.
//
//  - Keep connection idle at least |websocket_idle_ms| msec.
//    Checks WebSocket connection keep open in idle state.
//
//  - Wait for some message from server within
//    |websocket_receive_push_message_deadline_ms| msec, and echo it back.
//    Checks server can push a message after connection has been idle.
//
//  - Expect that |websocket_bye_message| message arrives within
//    |websocket_bye_deadline_ms| msec from server.
//    Checks previous message was sent to the server.
//
//  - Close the connection and wait |websocket_close_deadline_ms| msec
//    for onclose.
//    Checks WebSocket connection can be closed normally.

#ifndef CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_TASK_H_
#define CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_TASK_H_
#pragma once

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket.h"

namespace net {
class WebSocket;
}  // namespace net

namespace chrome_browser_net_websocket_experiment {

class WebSocketExperimentTask : public URLFetcher::Delegate,
                                public net::WebSocketDelegate {
 public:
  enum State {
    STATE_NONE,
    STATE_URL_FETCH,
    STATE_URL_FETCH_COMPLETE,
    STATE_WEBSOCKET_CONNECT,
    STATE_WEBSOCKET_CONNECT_COMPLETE,
    STATE_WEBSOCKET_SEND_HELLO,
    STATE_WEBSOCKET_RECV_HELLO,
    STATE_WEBSOCKET_KEEP_IDLE,
    STATE_WEBSOCKET_KEEP_IDLE_COMPLETE,
    STATE_WEBSOCKET_RECV_PUSH_MESSAGE,
    STATE_WEBSOCKET_ECHO_BACK_MESSAGE,
    STATE_WEBSOCKET_RECV_BYE,
    STATE_WEBSOCKET_CLOSE,
    STATE_WEBSOCKET_CLOSE_COMPLETE,
    NUM_STATES,
  };

  class Config {
   public:
    Config();
    ~Config();

    GURL url;
    std::string ws_protocol;
    std::string ws_origin;
    std::string ws_location;
    net::WebSocket::ProtocolVersion protocol_version;

    GURL http_url;

    int64 url_fetch_deadline_ms;
    int64 websocket_onopen_deadline_ms;
    std::string websocket_hello_message;
    int64 websocket_hello_echoback_deadline_ms;
    int64 websocket_idle_ms;
    int64 websocket_receive_push_message_deadline_ms;
    std::string websocket_bye_message;
    int64 websocket_bye_deadline_ms;
    int64 websocket_close_deadline_ms;
  };

  class Context {
   public:
    Context() {}
    virtual ~Context() {}

    virtual URLFetcher* CreateURLFetcher(
        const Config& config, URLFetcher::Delegate* delegate);
    virtual net::WebSocket* CreateWebSocket(
        const Config& config, net::WebSocketDelegate* delegate);

   private:
    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  class Result {
   public:
    Result()
        : last_result(net::OK),
          last_state(STATE_NONE) {}
    int last_result;
    State last_state;

    base::TimeDelta url_fetch;
    base::TimeDelta websocket_connect;
    base::TimeDelta websocket_echo;
    base::TimeDelta websocket_idle;
    base::TimeDelta websocket_total;
  };

  // WebSocketExperimentTask will call |callback| with the last status code
  // when the task is finished.
  WebSocketExperimentTask(const Config& config,
                          net::CompletionCallback* callback);
  virtual ~WebSocketExperimentTask();

  // Initializes histograms that WebSocketExperimentTask will use to save
  // results.  Must be called once before calling SaveResult().
  static void InitHistogram();

  // Releases histograms to store results.
  // Must be called after all WebSocketExperimentTasks are finished.
  static void ReleaseHistogram();

  void Run();
  void Cancel();
  void SaveResult() const;

  const Config& config() const { return config_; }
  const Result& result() const { return result_; }

  // URLFetcher::Delegate method.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // net::WebSocketDelegate methods
  virtual void OnOpen(net::WebSocket* websocket);
  virtual void OnMessage(net::WebSocket* websocket, const std::string& msg);
  virtual void OnError(net::WebSocket* websocket);
  virtual void OnClose(net::WebSocket* websocket, bool was_clean);
  virtual void OnSocketError(const net::WebSocket* websocket, int error);

  void SetContext(Context* context);

 private:
  void OnTimedOut();

  void DoLoop(int result);

  int DoURLFetch();
  int DoURLFetchComplete(int result);
  int DoWebSocketConnect();
  int DoWebSocketConnectComplete(int result);
  int DoWebSocketSendHello();
  int DoWebSocketReceiveHello(int result);
  int DoWebSocketKeepIdle();
  int DoWebSocketKeepIdleComplete(int result);
  int DoWebSocketReceivePushMessage(int result);
  int DoWebSocketEchoBackMessage();
  int DoWebSocketReceiveBye(int result);
  int DoWebSocketClose();
  int DoWebSocketCloseComplete(int result);
  void SetTimeout(int64 deadline_ms);
  void RevokeTimeoutTimer();
  void Finish(int result);

  Config config_;
  scoped_ptr<Context> context_;
  Result result_;

  ScopedRunnableMethodFactory<WebSocketExperimentTask> method_factory_;
  net::CompletionCallback* callback_;
  State next_state_;

  scoped_ptr<URLFetcher> url_fetcher_;
  base::TimeTicks url_fetch_start_time_;

  scoped_refptr<net::WebSocket> websocket_;
  int last_websocket_error_;
  std::deque<std::string> received_messages_;
  std::string push_message_;
  base::TimeTicks websocket_connect_start_time_;
  base::TimeTicks websocket_echo_start_time_;
  base::TimeTicks websocket_idle_start_time_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketExperimentTask);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_TASK_H_
