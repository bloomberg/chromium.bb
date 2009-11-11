// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_task.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket.h"

namespace chrome_browser_net_websocket_experiment {

URLFetcher* WebSocketExperimentTask::Context::CreateURLFetcher(
    const Config& config, URLFetcher::Delegate* delegate) {
  URLRequestContextGetter* getter =
      Profile::GetDefaultRequestContext();
  DCHECK(getter);
  DLOG(INFO) << "url=" << config.http_url;
  URLFetcher* fetcher =
      new URLFetcher(config.http_url, URLFetcher::GET, delegate);
  fetcher->set_request_context(getter);
  fetcher->set_load_flags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA);
  return fetcher;
}

net::WebSocket* WebSocketExperimentTask::Context::CreateWebSocket(
    const Config& config, net::WebSocketDelegate* delegate) {
  URLRequestContextGetter* getter =
      Profile::GetDefaultRequestContext();
  DCHECK(getter);
  net::WebSocket::Request* request(
      new net::WebSocket::Request(config.url,
                                  config.ws_protocol,
                                  config.ws_origin,
                                  config.ws_location,
                                  getter->GetURLRequestContext()));
  return new net::WebSocket(request, delegate);
}

WebSocketExperimentTask::WebSocketExperimentTask(
    const Config& config,
    net::CompletionCallback* callback)
    : config_(config),
      context_(ALLOW_THIS_IN_INITIALIZER_LIST(new Context())),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      callback_(callback),
      next_state_(STATE_NONE),
      last_websocket_error_(net::OK) {
}

WebSocketExperimentTask::~WebSocketExperimentTask() {
  DLOG(INFO) << "WebSocketExperimentTask finished";
  DCHECK(!websocket_);
}

void WebSocketExperimentTask::Run() {
  next_state_ = STATE_URL_FETCH;
  DoLoop(net::OK);
}

void WebSocketExperimentTask::Cancel() {
  next_state_ = STATE_NONE;
  DoLoop(net::OK);
}

// URLFetcher::Delegate method.
void WebSocketExperimentTask::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  result_.url_fetch = base::TimeTicks::Now() - url_fetch_start_time_;
  RevokeTimeoutTimer();
  DLOG(INFO) << "OnURLFetchCompleted";
  int result = net::ERR_FAILED;
  if (next_state_ != STATE_URL_FETCH_COMPLETE) {
    DLOG(INFO) << "unexpected state=" << next_state_;
    result = net::ERR_UNEXPECTED;
  } else if (response_code == 200 || response_code == 304)
    result = net::OK;
  DoLoop(result);
}

// net::WebSocketDelegate
void WebSocketExperimentTask::OnOpen(net::WebSocket* websocket) {
  result_.websocket_connect =
      base::TimeTicks::Now() - websocket_connect_start_time_;
  RevokeTimeoutTimer();
  int result = net::ERR_UNEXPECTED;
  if (next_state_ == STATE_WEBSOCKET_CONNECT_COMPLETE)
    result = net::OK;
  else
    DLOG(INFO) << "unexpected state=" << next_state_;
  DoLoop(result);
}

void WebSocketExperimentTask::OnMessage(
    net::WebSocket* websocket, const std::string& msg) {
  if (!result_.websocket_echo.ToInternalValue())
    result_.websocket_echo =
        base::TimeTicks::Now() - websocket_echo_start_time_;
  if (!websocket_idle_start_time_.is_null() &&
      !result_.websocket_idle.ToInternalValue())
    result_.websocket_idle =
        base::TimeTicks::Now() - websocket_idle_start_time_;
  RevokeTimeoutTimer();
  DLOG(INFO) << "OnMessage msg=" << msg;
  received_messages_.push_back(msg);
  int result = net::ERR_UNEXPECTED;
  switch (next_state_) {
    case STATE_WEBSOCKET_RECV_HELLO:
    case STATE_WEBSOCKET_RECV_PUSH_MESSAGE:
    case STATE_WEBSOCKET_RECV_BYE:
      result = net::OK;
      break;
    default:
      DLOG(INFO) << "unexpected state=" << next_state_;
      break;
  }
  DoLoop(result);
}

void WebSocketExperimentTask::OnClose(net::WebSocket* websocket) {
  RevokeTimeoutTimer();
  websocket_ = NULL;
  result_.websocket_total =
      base::TimeTicks::Now() - websocket_connect_start_time_;
  int result = net::ERR_CONNECTION_CLOSED;
  if (last_websocket_error_ != net::OK)
    result = last_websocket_error_;
  if (next_state_ == STATE_WEBSOCKET_CLOSE_COMPLETE)
    result = net::OK;
  DoLoop(result);
}

void WebSocketExperimentTask::OnError(
    const net::WebSocket* websocket, int error) {
  DLOG(INFO) << "WebSocket error=" << net::ErrorToString(error);
  last_websocket_error_ = error;
}

void WebSocketExperimentTask::SetContext(Context* context) {
  context_.reset(context);
}

void WebSocketExperimentTask::OnTimedOut() {
  DLOG(INFO) << "OnTimedOut";
  RevokeTimeoutTimer();
  DoLoop(net::ERR_TIMED_OUT);
}

void WebSocketExperimentTask::DoLoop(int result) {
  if (next_state_ == STATE_NONE) {
    Finish(net::ERR_ABORTED);
    return;
  }
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    DLOG(INFO) << "WebSocketExperimentTask state=" << state;
    switch (state) {
      case STATE_URL_FETCH:
        result = DoURLFetch();
        break;
      case STATE_URL_FETCH_COMPLETE:
        result = DoURLFetchComplete(result);
        break;
      case STATE_WEBSOCKET_CONNECT:
        result = DoWebSocketConnect();
        break;
      case STATE_WEBSOCKET_CONNECT_COMPLETE:
        result = DoWebSocketConnectComplete(result);
        break;
      case STATE_WEBSOCKET_SEND_HELLO:
        result = DoWebSocketSendHello();
        break;
      case STATE_WEBSOCKET_RECV_HELLO:
        result = DoWebSocketReceiveHello(result);
        break;
      case STATE_WEBSOCKET_KEEP_IDLE:
        result = DoWebSocketKeepIdle();
        break;
      case STATE_WEBSOCKET_KEEP_IDLE_COMPLETE:
        result = DoWebSocketKeepIdleComplete(result);
        break;
      case STATE_WEBSOCKET_RECV_PUSH_MESSAGE:
        result = DoWebSocketReceivePushMessage(result);
        break;
      case STATE_WEBSOCKET_ECHO_BACK_MESSAGE:
        result = DoWebSocketEchoBackMessage();
        break;
      case STATE_WEBSOCKET_RECV_BYE:
        result = DoWebSocketReceiveBye(result);
        break;
      case STATE_WEBSOCKET_CLOSE:
        result = DoWebSocketClose();
        break;
      case STATE_WEBSOCKET_CLOSE_COMPLETE:
        result = DoWebSocketCloseComplete(result);
        break;
      default:
        NOTREACHED();
        break;
    }
    result_.last_state = state;
  } while (result != net::ERR_IO_PENDING && next_state_ != STATE_NONE);

  DLOG(INFO) << "WebSocketExperiemntTask Loop done next_state=" << next_state_
             << " result=" << net::ErrorToString(result);
  if (result != net::ERR_IO_PENDING)
    Finish(result);
}

int WebSocketExperimentTask::DoURLFetch() {
  next_state_ = STATE_URL_FETCH_COMPLETE;
  DCHECK(!url_fetcher_.get());

  url_fetcher_.reset(context_->CreateURLFetcher(config_, this));
  SetTimeout(config_.url_fetch_deadline_ms);
  DLOG(INFO) << "URLFetch url=" << url_fetcher_->url()
             << " timeout=" << config_.url_fetch_deadline_ms;
  url_fetch_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoURLFetchComplete(int result) {
  DLOG(INFO) << "DoURLFetchComplete result=" << result;
  url_fetcher_.reset();

  if (result < 0)
    return result;

  next_state_ = STATE_WEBSOCKET_CONNECT;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketConnect() {
  DCHECK(!websocket_);

  next_state_ = STATE_WEBSOCKET_CONNECT_COMPLETE;
  websocket_ = context_->CreateWebSocket(config_, this);
  websocket_connect_start_time_ = base::TimeTicks::Now();
  websocket_->Connect();

  SetTimeout(config_.websocket_onopen_deadline_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketConnectComplete(int result) {
  if (result < 0)
    return result;
  DCHECK(websocket_);

  next_state_ = STATE_WEBSOCKET_SEND_HELLO;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketSendHello() {
  DCHECK(websocket_);

  next_state_ = STATE_WEBSOCKET_RECV_HELLO;

  websocket_echo_start_time_ = base::TimeTicks::Now();
  websocket_->Send(config_.websocket_hello_message);
  SetTimeout(config_.websocket_hello_echoback_deadline_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketReceiveHello(int result) {
  if (result < 0)
    return result;

  DCHECK(websocket_);

  if (received_messages_.size() != 1)
    return net::ERR_INVALID_RESPONSE;

  std::string msg = received_messages_.front();
  received_messages_.pop_front();
  if (msg != config_.websocket_hello_message)
    return net::ERR_INVALID_RESPONSE;

  next_state_ = STATE_WEBSOCKET_KEEP_IDLE;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketKeepIdle() {
  DCHECK(websocket_);

  next_state_ = STATE_WEBSOCKET_KEEP_IDLE_COMPLETE;
  websocket_idle_start_time_ = base::TimeTicks::Now();
  SetTimeout(config_.websocket_idle_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketKeepIdleComplete(int result) {
  if (result != net::ERR_TIMED_OUT) {
    // Server sends back too early, or unexpected close?
    if (result == net::OK)
      result = net::ERR_UNEXPECTED;
    return result;
  }

  DCHECK(websocket_);

  next_state_ = STATE_WEBSOCKET_RECV_PUSH_MESSAGE;
  SetTimeout(config_.websocket_receive_push_message_deadline_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketReceivePushMessage(int result) {
  if (result < 0)
    return result;

  DCHECK(websocket_);
  if (received_messages_.size() != 1)
    return net::ERR_INVALID_RESPONSE;

  push_message_ = received_messages_.front();
  received_messages_.pop_front();

  next_state_ = STATE_WEBSOCKET_ECHO_BACK_MESSAGE;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketEchoBackMessage() {
  DCHECK(websocket_);
  DCHECK(!push_message_.empty());

  next_state_ = STATE_WEBSOCKET_RECV_BYE;
  websocket_->Send(push_message_);
  SetTimeout(config_.websocket_bye_deadline_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketReceiveBye(int result) {
  if (result < 0)
    return result;

  DCHECK(websocket_);

  if (received_messages_.size() != 1)
    return net::ERR_INVALID_RESPONSE;

  std::string bye = received_messages_.front();
  received_messages_.pop_front();

  if (bye != config_.websocket_bye_message)
    return net::ERR_INVALID_RESPONSE;

  next_state_ = STATE_WEBSOCKET_CLOSE;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketClose() {
  DCHECK(websocket_);

  next_state_ = STATE_WEBSOCKET_CLOSE_COMPLETE;
  websocket_->Close();
  SetTimeout(config_.websocket_close_deadline_ms);
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoWebSocketCloseComplete(int result) {
  websocket_ = NULL;
  return result;
}

void WebSocketExperimentTask::SetTimeout(int64 deadline_ms) {
  bool r = ChromeThread::PostDelayedTask(
      ChromeThread::IO,
      FROM_HERE,
      method_factory_.NewRunnableMethod(&WebSocketExperimentTask::OnTimedOut),
      deadline_ms);
  DCHECK(r) << "No IO thread running?";
}

void WebSocketExperimentTask::RevokeTimeoutTimer() {
  method_factory_.RevokeAll();
}

void WebSocketExperimentTask::Finish(int result) {
  DLOG(INFO) << "Finish Task for " << config_.url
             << " next_state=" << next_state_
             << " result=" << net::ErrorToString(result);
  url_fetcher_.reset();
  scoped_refptr<net::WebSocket> websocket = websocket_;
  websocket_ = NULL;
  callback_->Run(result);
  if (websocket)
    websocket->DetachDelegate();
}

}  // namespace chrome_browser_net
