// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"

#include "base/compiler_specific.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "net/base/net_errors.h"

namespace chrome_browser_net_websocket_experiment {

static const char *kExperimentHost = "websocket-experiment.chromium.org";
static const int kAlternativePort = 61985;

static const char kLastStateName[] = "LastState";
static const char kUrlFetchName[] = "UrlFetch";
static const char kWebSocketConnectName[] = "WebSocketConnect";
static const char kWebSocketEchoName[] = "WebSocketEcho";
static const char kWebSocketIdleName[] = "WebSocketIdle";
static const char kWebSocketTotalName[] = "WebSocketTotal";
static const int kUrlFetchDeadlineSec = 10;
static const int kWebSocketConnectDeadlineSec = 10;
static const int kWebSocketEchoDeadlineSec = 5;
static const int kWebSocketIdleSec = 1;
static const int kWebSocketPushDeadlineSec = 1;
static const int kWebSocketByeDeadlineSec = 10;
static const int kWebSocketCloseDeadlineSec = 5;
static const int kWebSocketTimeSec = 10;

// Hold reference while experiment is running.
static scoped_refptr<WebSocketExperimentRunner> runner;

/* static */
void WebSocketExperimentRunner::Start() {
  DCHECK(!runner.get());
  runner = new WebSocketExperimentRunner;
  runner->Run();
}

/* static */
void WebSocketExperimentRunner::Stop() {
  if (runner.get())
    runner->Cancel();
  runner = NULL;
}

WebSocketExperimentRunner::WebSocketExperimentRunner()
    : next_state_(STATE_NONE),
      task_state_(STATE_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          task_callback_(this, &WebSocketExperimentRunner::OnTaskCompleted)) {
  InitConfig();
  InitHistograms();
}

WebSocketExperimentRunner::~WebSocketExperimentRunner() {
  DLOG(INFO) << "WebSocketExperimentRunner deleted";
  DCHECK(!task_.get());
}

void WebSocketExperimentRunner::Run() {
  DCHECK_EQ(next_state_, STATE_NONE);
  next_state_ = STATE_RUN_WS;
  ChromeThread::PostDelayedTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
      config_.initial_delay_ms);
}

void WebSocketExperimentRunner::Cancel() {
  next_state_ = STATE_NONE;
  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop));
}

void WebSocketExperimentRunner::InitConfig() {
  config_.initial_delay_ms = 5 * 60 * 1000;  // 5 mins
  config_.next_delay_ms = 12 * 60 * 60 * 1000;  // 12 hours

  WebSocketExperimentTask::Config task_config;
  task_config.ws_protocol = "google-websocket-liveexperiment";
  task_config.ws_origin = "http://dev.chromium.org/";
  task_config.url_fetch_deadline_ms = kUrlFetchDeadlineSec * 1000;
  task_config.websocket_onopen_deadline_ms =
      kWebSocketConnectDeadlineSec * 1000;
  task_config.websocket_hello_message = "Hello";
  task_config.websocket_hello_echoback_deadline_ms =
      kWebSocketEchoDeadlineSec * 1000;
  // Note: wait 1.5 sec in websocket_experiment_def.txt
  task_config.websocket_idle_ms = kWebSocketIdleSec * 1000;
  task_config.websocket_receive_push_message_deadline_ms =
      kWebSocketPushDeadlineSec * 1000;
  task_config.websocket_bye_message = "Bye";
  task_config.websocket_bye_deadline_ms =
      kWebSocketByeDeadlineSec * 1000;
  task_config.websocket_close_deadline_ms =
      kWebSocketCloseDeadlineSec * 1000;

  config_.ws_config = task_config;
  config_.ws_config.url =
      GURL(StringPrintf("ws://%s/live_exp", kExperimentHost));
  config_.ws_config.ws_location =
      StringPrintf("ws://%s/live_exp", kExperimentHost);
  config_.ws_config.http_url =
      GURL(StringPrintf("http://%s/", kExperimentHost));

  config_.wss_config = task_config;
  config_.wss_config.url =
      GURL(StringPrintf("wss://%s/live_exp", kExperimentHost));
  config_.wss_config.ws_location =
      StringPrintf("wss://%s/live_exp", kExperimentHost);
  config_.wss_config.http_url =
      GURL(StringPrintf("https://%s/", kExperimentHost));

  config_.ws_nondefault_config = task_config;
  config_.ws_nondefault_config.url =
      GURL(StringPrintf("ws://%s:%d/live_exp",
                        kExperimentHost, kAlternativePort));
  config_.ws_nondefault_config.ws_location =
      StringPrintf("ws://%s:%d/live_exp",
                   kExperimentHost, kAlternativePort);
  config_.ws_nondefault_config.http_url =
      GURL(StringPrintf("http://%s:%d/",
                        kExperimentHost, kAlternativePort));
}

void WebSocketExperimentRunner::InitHistograms() {
  InitHistogram<LinearHistogram, Histogram::Sample>(
      kLastStateName, 1, WebSocketExperimentTask::NUM_STATES,
      WebSocketExperimentTask::NUM_STATES + 1);

  InitHistogram<Histogram, base::TimeDelta>(
      kUrlFetchName, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(kUrlFetchDeadlineSec), 50);

  InitHistogram<Histogram, base::TimeDelta>(
      kWebSocketConnectName, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(kWebSocketConnectDeadlineSec), 50);

  InitHistogram<Histogram, base::TimeDelta>(
      kWebSocketEchoName, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(kWebSocketEchoDeadlineSec), 50);

  InitHistogram<Histogram, base::TimeDelta>(
      kWebSocketIdleName, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(
          kWebSocketIdleSec + kWebSocketPushDeadlineSec), 50);

  InitHistogram<Histogram, base::TimeDelta>(
      kWebSocketTotalName, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromSeconds(kWebSocketTimeSec), 50);
}

void WebSocketExperimentRunner::DoLoop() {
  if (next_state_ == STATE_NONE) {
    if (task_.get()) {
      AddRef();  // Release in OnTaskCompleted.
      task_->Cancel();
    }
    return;
  }

  State state = next_state_;
  task_state_ = STATE_NONE;
  next_state_ = STATE_NONE;

  DLOG(INFO) << "WebSocketExperiment state=" << state;
  switch (state) {
    case STATE_IDLE:
      task_.reset();
      next_state_ = STATE_RUN_WS;
      ChromeThread::PostDelayedTask(
          ChromeThread::IO,
          FROM_HERE,
          NewRunnableMethod(this, &WebSocketExperimentRunner::DoLoop),
          config_.next_delay_ms);
      break;
    case STATE_RUN_WS:
      task_.reset(new WebSocketExperimentTask(config_.ws_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WS;
      next_state_ = STATE_RUN_WSS;
      break;
    case STATE_RUN_WSS:
      task_.reset(new WebSocketExperimentTask(config_.wss_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WSS;
      next_state_ = STATE_RUN_WS_NODEFAULT_PORT;
      break;
    case STATE_RUN_WS_NODEFAULT_PORT:
      task_.reset(new WebSocketExperimentTask(config_.ws_nondefault_config,
                                              &task_callback_));
      task_state_ = STATE_RUN_WS_NODEFAULT_PORT;
      next_state_ = STATE_IDLE;
      break;
    default:
      NOTREACHED();
      break;
  }
  if (task_.get())
    task_->Run();
}

void WebSocketExperimentRunner::OnTaskCompleted(int result) {
  DLOG(INFO) << "WebSocketExperiment TaskCompleted result="
             << net::ErrorToString(result);
  if (result == net::ERR_ABORTED) {
    task_.reset();
    // Task is Canceled.
    Release();
    return;
  }
  UpdateTaskResultHistogram(task_.get());
  task_.reset();

  DoLoop();
}

void WebSocketExperimentRunner::UpdateTaskResultHistogram(
    const WebSocketExperimentTask* task) {
  DCHECK(task);
  const WebSocketExperimentTask::Config& task_config = task->config();
  const WebSocketExperimentTask::Result& task_result = task->result();
  DLOG(INFO) << "Result for url=" << task_config.url
             << " last_result="
             << net::ErrorToString(task_result.last_result)
             << " last_state=" << task_result.last_state
             << " url_fetch=" << task_result.url_fetch.InSecondsF()
             << " websocket_connect="
             << task_result.websocket_connect.InSecondsF()
             << " websocket_echo="
             << task_result.websocket_echo.InSecondsF()
             << " websocket_idle="
             << task_result.websocket_idle.InSecondsF()
             << " websocket_total="
             << task_result.websocket_total.InSecondsF();


  Histogram* last_state = GetHistogram(kLastStateName);
  DCHECK(last_state);
  last_state->Add(task_result.last_state);

  Histogram* url_fetch = GetHistogram(kUrlFetchName);
  DCHECK(url_fetch);
  url_fetch->AddTime(task_result.url_fetch);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_CONNECT_COMPLETE)
    return;

  Histogram* websocket_connect = GetHistogram(kWebSocketConnectName);
  DCHECK(websocket_connect);
  websocket_connect->AddTime(task_result.websocket_connect);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_RECV_HELLO)
    return;

  Histogram* websocket_echo = GetHistogram(kWebSocketEchoName);
  DCHECK(websocket_echo);
  websocket_echo->AddTime(task_result.websocket_echo);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_KEEP_IDLE)
    return;

  Histogram* websocket_idle = GetHistogram(kWebSocketIdleName);
  DCHECK(websocket_idle);
  websocket_idle->AddTime(task_result.websocket_idle);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_CLOSE_COMPLETE)
    return;

  Histogram* websocket_total = GetHistogram(kWebSocketTotalName);
  DCHECK(websocket_total);
  websocket_total->AddTime(task_result.websocket_total);
}

template<class HistogramType, typename HistogramSample>
void WebSocketExperimentRunner::InitHistogram(
    const char* type_name,
    HistogramSample min, HistogramSample max, size_t bucket_count) {
  static std::string name = StringPrintf(
      "WebSocketExperiment.Basic.%s", type_name);
  HistogramType* counter = new HistogramType(name.c_str(),
                                             min, max, bucket_count);
  counter->SetFlags(kUmaTargetedHistogramFlag);
  ws_histograms_[type_name] = linked_ptr<Histogram>(counter);

  name = StringPrintf("WebSocketExperiment.Secure.%s", type_name);
  counter = new HistogramType(name.c_str(), min, max, bucket_count);
  counter->SetFlags(kUmaTargetedHistogramFlag);
  wss_histograms_[type_name] = linked_ptr<Histogram>(counter);

  name = StringPrintf("WebSocketExperiment.NoDefaultPort.%s", type_name);
  counter = new HistogramType(name.c_str(), min, max, bucket_count);
  counter->SetFlags(kUmaTargetedHistogramFlag);
  ws_nondefault_histograms_[type_name] = linked_ptr<Histogram>(counter);
}

Histogram* WebSocketExperimentRunner::GetHistogram(
    const std::string& type_name) const {
  const HistogramMap* histogram_map = NULL;
  switch (task_state_) {
    case STATE_RUN_WS:
      histogram_map = &ws_histograms_;
      break;
    case STATE_RUN_WSS:
      histogram_map = &wss_histograms_;
      break;
    case STATE_RUN_WS_NODEFAULT_PORT:
      histogram_map = &ws_nondefault_histograms_;
      break;
    default:
      NOTREACHED();
      return NULL;
  }
  DCHECK(histogram_map);
  HistogramMap::const_iterator found = histogram_map->find(type_name);
  DCHECK(found != histogram_map->end());
  return found->second.get();
}

}  // namespace chrome_browser_net_websocket_experiment
