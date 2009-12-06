// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/websocket_experiment/websocket_experiment_runner.h"

#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/histogram.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "net/base/net_errors.h"

namespace chrome_browser_net_websocket_experiment {

static const char *kExperimentHost = "websocket-experiment.chromium.org";
static const int kAlternativePort = 61985;

static const int kUrlFetchDeadlineSec = 10;
static const int kWebSocketConnectDeadlineSec = 10;
static const int kWebSocketEchoDeadlineSec = 5;
static const int kWebSocketIdleSec = 1;
static const int kWebSocketPushDeadlineSec = 1;
static const int kWebSocketByeDeadlineSec = 10;
static const int kWebSocketCloseDeadlineSec = 5;
static const int kWebSocketTimeSec = 10;
static const int kTimeBucketCount = 50;

// TODO(ukai): Use new thread-safe-reference-counted Histograms.
#define UPDATE_HISTOGRAM(name, sample, min, max, bucket_count) do {     \
    switch (task_state_) {                                              \
      case STATE_RUN_WS:                                                \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              LinearHistogram::LinearHistogramFactoryGet(\
                  "WebSocketExperiment.Basic." name,                    \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->Add(sample);                                         \
        }                                                               \
        break;                                                          \
      case STATE_RUN_WSS:                                               \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              LinearHistogram::LinearHistogramFactoryGet(\
                  "WebSocketExperiment.Secure." name,                   \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->Add(sample);                                         \
        }                                                               \
        break;                                                          \
      case STATE_RUN_WS_NODEFAULT_PORT:                                 \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              LinearHistogram::LinearHistogramFactoryGet(\
                  "WebSocketExperiment.NoDefaultPort." name,            \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->Add(sample);                                         \
        }                                                               \
        break;                                                          \
      default:                                                          \
        NOTREACHED();                                                   \
        break;                                                          \
    }                                                                   \
  } while (0)

#define UPDATE_HISTOGRAM_TIMES(name, sample, min, max, bucket_count) do { \
    switch (task_state_) {                                              \
      case STATE_RUN_WS:                                                \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              Histogram::HistogramFactoryGet(\
                  "WebSocketExperiment.Basic." name,                    \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->AddTime(sample);                                     \
        }                                                               \
        break;                                                          \
      case STATE_RUN_WSS:                                               \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              Histogram::HistogramFactoryGet(\
                  "WebSocketExperiment.Secure." name,                   \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->AddTime(sample);                                     \
        }                                                               \
        break;                                                          \
      case STATE_RUN_WS_NODEFAULT_PORT:                                 \
        {                                                               \
          static scoped_refptr<Histogram> counter =                     \
              Histogram::HistogramFactoryGet(\
                  "WebSocketExperiment.NoDefaultPort." name,            \
                  min, max, bucket_count);                              \
          counter->SetFlags(kUmaTargetedHistogramFlag);                 \
          counter->AddTime(sample);                                     \
        }                                                               \
        break;                                                          \
      default:                                                          \
        NOTREACHED();                                                   \
        break;                                                          \
    }                                                                   \
  } while (0);

// Hold reference while experiment is running.
static scoped_refptr<WebSocketExperimentRunner> runner;

/* static */
void WebSocketExperimentRunner::Start() {
  DCHECK(!runner.get());

  scoped_refptr<FieldTrial> trial = new FieldTrial("WebSocketExperiment", 1000);
  trial->AppendGroup("_active", 5);  // 0.5% in _active group.

  if (trial->group() == FieldTrial::kNotParticipating)
    return;

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
}

WebSocketExperimentRunner::~WebSocketExperimentRunner() {
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
  if (result == net::ERR_ABORTED) {
    task_.reset();
    // Task is Canceled.
    DLOG(INFO) << "WebSocketExperiment Task is canceled.";
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
  const WebSocketExperimentTask::Result& task_result = task->result();

  UPDATE_HISTOGRAM("LastState", task_result.last_state,
                   1, WebSocketExperimentTask::NUM_STATES,
                   WebSocketExperimentTask::NUM_STATES + 1);

  UPDATE_HISTOGRAM_TIMES("UrlFetch", task_result.url_fetch,
                         base::TimeDelta::FromMilliseconds(1),
                         base::TimeDelta::FromSeconds(kUrlFetchDeadlineSec),
                         kTimeBucketCount);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_CONNECT_COMPLETE)
    return;

  UPDATE_HISTOGRAM_TIMES("WebSocketConnect", task_result.websocket_connect,
                         base::TimeDelta::FromMilliseconds(1),
                         base::TimeDelta::FromSeconds(
                             kWebSocketConnectDeadlineSec),
                         kTimeBucketCount);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_RECV_HELLO)
    return;

  UPDATE_HISTOGRAM_TIMES("WebSocketEcho", task_result.websocket_echo,
                         base::TimeDelta::FromMilliseconds(1),
                         base::TimeDelta::FromSeconds(
                             kWebSocketEchoDeadlineSec),
                         kTimeBucketCount);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_KEEP_IDLE)
    return;

  UPDATE_HISTOGRAM_TIMES("WebSocketIdle", task_result.websocket_idle,
                         base::TimeDelta::FromMilliseconds(1),
                         base::TimeDelta::FromSeconds(
                             kWebSocketIdleSec + kWebSocketPushDeadlineSec),
                         kTimeBucketCount);

  if (task_result.last_state <
      WebSocketExperimentTask::STATE_WEBSOCKET_CLOSE_COMPLETE)
    return;

  UPDATE_HISTOGRAM_TIMES("WebSocketTotal", task_result.websocket_total,
                         base::TimeDelta::FromMilliseconds(1),
                         base::TimeDelta::FromSeconds(kWebSocketTimeSec),
                         kTimeBucketCount);
}

}  // namespace chrome_browser_net_websocket_experiment
