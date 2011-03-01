// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_task.h"

#include "base/hash_tables.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "net/base/host_resolver.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket.h"

using base::Histogram;
using base::LinearHistogram;

namespace chrome_browser_net_websocket_experiment {

static std::string GetProtocolVersionName(
    net::WebSocket::ProtocolVersion protocol_version) {
  switch (protocol_version) {
    case net::WebSocket::DEFAULT_VERSION:
      return "default protocol";
    case net::WebSocket::DRAFT75:
      return "draft 75 protocol";
    default:
      NOTREACHED();
  }
  return "";
}

URLFetcher* WebSocketExperimentTask::Context::CreateURLFetcher(
    const Config& config, URLFetcher::Delegate* delegate) {
  URLRequestContextGetter* getter =
      Profile::GetDefaultRequestContext();
  // Profile::GetDefaultRequestContext() is initialized lazily, on the UI
  // thread. So here, where we access it from the IO thread, if the task runs
  // before it has gotten lazily initialized yet.
  if (!getter)
    return NULL;
  URLFetcher* fetcher =
      new URLFetcher(config.http_url, URLFetcher::GET, delegate);
  fetcher->set_request_context(getter);
  fetcher->set_load_flags(
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA |
      net::LOAD_IGNORE_CERT_AUTHORITY_INVALID);
  return fetcher;
}

net::WebSocket* WebSocketExperimentTask::Context::CreateWebSocket(
    const Config& config, net::WebSocketDelegate* delegate) {
  URLRequestContextGetter* getter =
      Profile::GetDefaultRequestContext();
  // Profile::GetDefaultRequestContext() is initialized lazily, on the UI
  // thread. So here, where we access it from the IO thread, if the task runs
  // before it has gotten lazily initialized yet.
  if (!getter)
    return NULL;
  net::WebSocket::Request* request(
      new net::WebSocket::Request(config.url,
                                  config.ws_protocol,
                                  config.ws_origin,
                                  config.ws_location,
                                  config.protocol_version,
                                  getter->GetURLRequestContext()));
  return new net::WebSocket(request, delegate);
}

// Expects URL Fetch and WebSocket connection handshake will finish in
// a few seconds.
static const int kUrlFetchDeadlineSec = 10;
static const int kWebSocketConnectDeadlineSec = 10;
// Expects WebSocket live experiment server echoes message back within a few
// seconds.
static const int kWebSocketEchoDeadlineSec = 5;
// WebSocket live experiment server keeps idle for 1.5 seconds and sends
// a message.  So, expects idle for at least 1 second and expects message
// arrives within 1 second after that.
static const int kWebSocketIdleSec = 1;
static const int kWebSocketPushDeadlineSec = 1;
// WebSocket live experiment server sends "bye" message soon.
static const int kWebSocketByeDeadlineSec = 10;
// WebSocket live experiment server closes after it receives "bye" message.
static const int kWebSocketCloseDeadlineSec = 5;

// All of above are expected within a few seconds.  We'd like to see time
// distribution between 0 to 10 seconds.
static const int kWebSocketTimeSec = 10;
static const int kTimeBucketCount = 50;

// Holds Histogram objects during experiments run.
static base::hash_map<std::string, Histogram*>* g_histogram_table;

WebSocketExperimentTask::Config::Config()
    : ws_protocol("google-websocket-liveexperiment"),
      ws_origin("http://dev.chromium.org/"),
      protocol_version(net::WebSocket::DEFAULT_VERSION),
      url_fetch_deadline_ms(kUrlFetchDeadlineSec * 1000),
      websocket_onopen_deadline_ms(kWebSocketConnectDeadlineSec * 1000),
      websocket_hello_message("Hello"),
      websocket_hello_echoback_deadline_ms(kWebSocketEchoDeadlineSec * 1000),
      // Note: websocket live experiment server is configured to wait 1.5 sec
      // in websocket_experiment_def.txt on server.  So, client expects idle
      // at least 1 sec and expects a message arrival within next 1 sec.
      websocket_idle_ms(kWebSocketIdleSec * 1000),
      websocket_receive_push_message_deadline_ms(
          kWebSocketPushDeadlineSec * 1000),
      websocket_bye_message("Bye"),
      websocket_bye_deadline_ms(kWebSocketByeDeadlineSec * 1000),
      websocket_close_deadline_ms(kWebSocketCloseDeadlineSec * 1000) {
}

WebSocketExperimentTask::Config::~Config() {}

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
  DCHECK(!websocket_);
}

/* static */
void WebSocketExperimentTask::InitHistogram() {
  DCHECK(!g_histogram_table);
  g_histogram_table = new base::hash_map<std::string, Histogram*>;
}

static std::string GetCounterNameForConfig(
    const WebSocketExperimentTask::Config& config, const std::string& name) {
  std::string protocol_version = "";
  switch (config.protocol_version) {
    case net::WebSocket::DEFAULT_VERSION:
      protocol_version = "Draft76";
      break;
    case net::WebSocket::DRAFT75:
      protocol_version = "";
      break;
    default:
      NOTREACHED();
  }
  if (config.url.SchemeIs("wss")) {
    return "WebSocketExperiment.Secure" + protocol_version + "." + name;
  } else if (config.url.has_port() && config.url.IntPort() != 80) {
    return "WebSocketExperiment.NoDefaultPort" + protocol_version + "." + name;
  } else {
    return "WebSocketExperiment.Basic" + protocol_version + "." + name;
  }
}

static Histogram* GetEnumsHistogramForConfig(
    const WebSocketExperimentTask::Config& config,
    const std::string& name,
    Histogram::Sample boundary_value) {
  DCHECK(g_histogram_table);
  std::string counter_name = GetCounterNameForConfig(config, name);
  base::hash_map<std::string, Histogram*>::iterator found =
      g_histogram_table->find(counter_name);
  if (found != g_histogram_table->end()) {
    return found->second;
  }
  Histogram* counter = LinearHistogram::FactoryGet(
      counter_name, 1, boundary_value, boundary_value + 1,
      Histogram::kUmaTargetedHistogramFlag);
  counter->AddRef();  // Released in ReleaseHistogram().
  g_histogram_table->insert(std::make_pair(counter_name, counter));
  return counter;
}

static Histogram* GetTimesHistogramForConfig(
    const WebSocketExperimentTask::Config& config,
    const std::string& name,
    base::TimeDelta min,
    base::TimeDelta max,
    size_t bucket_count) {
  DCHECK(g_histogram_table);
  std::string counter_name = GetCounterNameForConfig(config, name);
  base::hash_map<std::string, Histogram*>::iterator found =
      g_histogram_table->find(counter_name);
  if (found != g_histogram_table->end()) {
    return found->second;
  }
  Histogram* counter = Histogram::FactoryTimeGet(
      counter_name, min, max, bucket_count,
      Histogram::kUmaTargetedHistogramFlag);
  counter->AddRef();  // Released in ReleaseHistogram().
  g_histogram_table->insert(std::make_pair(counter_name, counter));
  return counter;
}

static void UpdateHistogramEnums(
    const WebSocketExperimentTask::Config& config,
    const std::string& name,
    Histogram::Sample sample,
    Histogram::Sample boundary_value) {
  Histogram* counter = GetEnumsHistogramForConfig(config, name, boundary_value);
  counter->Add(sample);
}

static void UpdateHistogramTimes(
    const WebSocketExperimentTask::Config& config,
    const std::string& name,
    base::TimeDelta sample,
    base::TimeDelta min,
    base::TimeDelta max,
    size_t bucket_count) {
  Histogram* counter = GetTimesHistogramForConfig(
      config, name, min, max, bucket_count);
  counter->AddTime(sample);
}

/* static */
void WebSocketExperimentTask::ReleaseHistogram() {
  DCHECK(g_histogram_table);
  for (base::hash_map<std::string, Histogram*>::iterator iter =
           g_histogram_table->begin();
       iter != g_histogram_table->end();
       ++iter) {
    Histogram* counter = iter->second;
    if (counter != NULL)
      counter->Release();
    iter->second = NULL;
  }
  delete g_histogram_table;
  g_histogram_table = NULL;
}

void WebSocketExperimentTask::Run() {
  DVLOG(1) << "Run WebSocket experiment for " << config_.url << " "
           << GetProtocolVersionName(config_.protocol_version);
  next_state_ = STATE_URL_FETCH;
  DoLoop(net::OK);
}

void WebSocketExperimentTask::Cancel() {
  next_state_ = STATE_NONE;
  DoLoop(net::OK);
}

void WebSocketExperimentTask::SaveResult() const {
  DVLOG(1) << "WebSocket experiment save result for " << config_.url
           << " last_state=" << result_.last_state;
  UpdateHistogramEnums(config_, "LastState", result_.last_state, NUM_STATES);
  UpdateHistogramTimes(config_, "UrlFetch", result_.url_fetch,
                       base::TimeDelta::FromMilliseconds(1),
                       base::TimeDelta::FromSeconds(kUrlFetchDeadlineSec),
                       kTimeBucketCount);

  if (result_.last_state < STATE_WEBSOCKET_CONNECT_COMPLETE)
    return;

  UpdateHistogramTimes(config_, "WebSocketConnect", result_.websocket_connect,
                       base::TimeDelta::FromMilliseconds(1),
                       base::TimeDelta::FromSeconds(
                           kWebSocketConnectDeadlineSec),
                       kTimeBucketCount);

  if (result_.last_state < STATE_WEBSOCKET_RECV_HELLO)
    return;

  UpdateHistogramTimes(config_, "WebSocketEcho", result_.websocket_echo,
                       base::TimeDelta::FromMilliseconds(1),
                       base::TimeDelta::FromSeconds(
                           kWebSocketEchoDeadlineSec),
                       kTimeBucketCount);

  if (result_.last_state < STATE_WEBSOCKET_KEEP_IDLE)
    return;

  UpdateHistogramTimes(config_, "WebSocketIdle", result_.websocket_idle,
                       base::TimeDelta::FromMilliseconds(1),
                       base::TimeDelta::FromSeconds(
                           kWebSocketIdleSec + kWebSocketPushDeadlineSec),
                       kTimeBucketCount);

  if (result_.last_state < STATE_WEBSOCKET_CLOSE_COMPLETE)
    return;

  UpdateHistogramTimes(config_, "WebSocketTotal", result_.websocket_total,
                       base::TimeDelta::FromMilliseconds(1),
                       base::TimeDelta::FromSeconds(kWebSocketTimeSec),
                       kTimeBucketCount);
}

// URLFetcher::Delegate method.
void WebSocketExperimentTask::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  result_.url_fetch = base::TimeTicks::Now() - url_fetch_start_time_;
  RevokeTimeoutTimer();
  int result = net::ERR_FAILED;
  if (next_state_ != STATE_URL_FETCH_COMPLETE) {
    DVLOG(1) << "unexpected state=" << next_state_
             << " at OnURLFetchComplete for " << config_.http_url;
    result = net::ERR_UNEXPECTED;
  } else if (response_code == 200 || response_code == 304) {
    result = net::OK;
  }
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
    DVLOG(1) << "unexpected state=" << next_state_
             << " at OnOpen for " << config_.url
             << " " << GetProtocolVersionName(config_.protocol_version);
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
  received_messages_.push_back(msg);
  int result = net::ERR_UNEXPECTED;
  switch (next_state_) {
    case STATE_WEBSOCKET_RECV_HELLO:
    case STATE_WEBSOCKET_RECV_PUSH_MESSAGE:
    case STATE_WEBSOCKET_RECV_BYE:
      result = net::OK;
      break;
    default:
      DVLOG(1) << "unexpected state=" << next_state_
               << " at OnMessage for " << config_.url
               << " " << GetProtocolVersionName(config_.protocol_version);
      break;
  }
  DoLoop(result);
}

void WebSocketExperimentTask::OnError(net::WebSocket* websocket) {
  // TODO(ukai): record error count?
}

void WebSocketExperimentTask::OnClose(
    net::WebSocket* websocket, bool was_clean) {
  RevokeTimeoutTimer();
  websocket_ = NULL;
  result_.websocket_total =
      base::TimeTicks::Now() - websocket_connect_start_time_;
  int result = net::ERR_CONNECTION_CLOSED;
  if (last_websocket_error_ != net::OK)
    result = last_websocket_error_;
  DVLOG(1) << "WebSocket onclose was_clean=" << was_clean
           << " next_state=" << next_state_
           << " last_error=" << net::ErrorToString(result);
  if (config_.protocol_version == net::WebSocket::DEFAULT_VERSION) {
    if (next_state_ == STATE_WEBSOCKET_CLOSE_COMPLETE && was_clean)
      result = net::OK;
  } else {
    // DRAFT75 doesn't report was_clean correctly.
    if (next_state_ == STATE_WEBSOCKET_CLOSE_COMPLETE)
      result = net::OK;
  }
  DoLoop(result);
}

void WebSocketExperimentTask::OnSocketError(
    const net::WebSocket* websocket, int error) {
  DVLOG(1) << "WebSocket socket level error=" << net::ErrorToString(error)
           << " next_state=" << next_state_
           << " for " << config_.url
           << " " << GetProtocolVersionName(config_.protocol_version);
  last_websocket_error_ = error;
}

void WebSocketExperimentTask::SetContext(Context* context) {
  context_.reset(context);
}

void WebSocketExperimentTask::OnTimedOut() {
  DVLOG(1) << "OnTimedOut next_state=" << next_state_
           << " for " << config_.url
           << " " << GetProtocolVersionName(config_.protocol_version);
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

  if (result != net::ERR_IO_PENDING)
    Finish(result);
}

int WebSocketExperimentTask::DoURLFetch() {
  DCHECK(!url_fetcher_.get());

  url_fetcher_.reset(context_->CreateURLFetcher(config_, this));
  if (!url_fetcher_.get()) {
    // Request context is not ready.
    next_state_ = STATE_NONE;
    return net::ERR_UNEXPECTED;
  }

  next_state_ = STATE_URL_FETCH_COMPLETE;
  SetTimeout(config_.url_fetch_deadline_ms);
  url_fetch_start_time_ = base::TimeTicks::Now();
  url_fetcher_->Start();
  return net::ERR_IO_PENDING;
}

int WebSocketExperimentTask::DoURLFetchComplete(int result) {
  url_fetcher_.reset();

  if (result < 0)
    return result;

  next_state_ = STATE_WEBSOCKET_CONNECT;
  return net::OK;
}

int WebSocketExperimentTask::DoWebSocketConnect() {
  DCHECK(!websocket_);

  websocket_ = context_->CreateWebSocket(config_, this);
  if (!websocket_) {
    // Request context is not ready.
    next_state_ = STATE_NONE;
    return net::ERR_UNEXPECTED;
  }
  next_state_ = STATE_WEBSOCKET_CONNECT_COMPLETE;
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
  bool r = BrowserThread::PostDelayedTask(
      BrowserThread::IO,
      FROM_HERE,
      method_factory_.NewRunnableMethod(&WebSocketExperimentTask::OnTimedOut),
      deadline_ms);
  DCHECK(r) << "No IO thread running?";
}

void WebSocketExperimentTask::RevokeTimeoutTimer() {
  method_factory_.RevokeAll();
}

void WebSocketExperimentTask::Finish(int result) {
  url_fetcher_.reset();
  scoped_refptr<net::WebSocket> websocket = websocket_;
  websocket_ = NULL;
  if (websocket)
    websocket->DetachDelegate();
  DVLOG(1) << "Finish WebSocket experiment for " << config_.url
           << " " << GetProtocolVersionName(config_.protocol_version)
           << " next_state=" << next_state_
           << " result=" << net::ErrorToString(result);
  callback_->Run(result);  // will release this.
}

}  // namespace chrome_browser_net
