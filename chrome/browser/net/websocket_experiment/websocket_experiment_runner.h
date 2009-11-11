// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_
#define CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_

#include "base/basictypes.h"
#include "base/histogram.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/net/websocket_experiment/websocket_experiment_task.h"
#include "net/base/completion_callback.h"

namespace chrome_browser_net_websocket_experiment {

class WebSocketExperimentRunner
    : public base::RefCountedThreadSafe<WebSocketExperimentRunner> {
 public:
  class Config {
   public:
    Config()
        : initial_delay_ms(0),
          next_delay_ms(0) {}

    int64 initial_delay_ms;
    int64 next_delay_ms;
    WebSocketExperimentTask::Config ws_config;
    WebSocketExperimentTask::Config wss_config;
    WebSocketExperimentTask::Config ws_nondefault_config;
  };
  static void Start();
  static void Stop();

  void Run();
  void Cancel();

 private:
  typedef std::map<std::string, linked_ptr<Histogram> > HistogramMap;

  enum State {
    STATE_NONE,
    STATE_IDLE,
    STATE_RUN_WS,
    STATE_RUN_WSS,
    STATE_RUN_WS_NODEFAULT_PORT,
  };
  WebSocketExperimentRunner();
  virtual ~WebSocketExperimentRunner();
  friend class base::RefCountedThreadSafe<WebSocketExperimentRunner>;

  void InitConfig();
  void InitHistograms();
  void DoLoop();
  void OnTaskCompleted(int result);
  void UpdateTaskResultHistogram(const WebSocketExperimentTask* task);

  template<class HistogramType, typename HistogramSample>
  void InitHistogram(const char* type_name,
                     HistogramSample min, HistogramSample max,
                     size_t bucket_count);

  Histogram* GetHistogram(const std::string& type_name) const;

  Config config_;
  State next_state_;
  State task_state_;
  scoped_ptr<WebSocketExperimentTask> task_;
  net::CompletionCallbackImpl<WebSocketExperimentRunner> task_callback_;

  HistogramMap ws_histograms_;
  HistogramMap wss_histograms_;
  HistogramMap ws_nondefault_histograms_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketExperimentRunner);
};

}  // namespace chrome_browser_net_websocket_experiment

#endif  // CHROME_BROWSER_NET_WEBSOCKET_EXPERIMENT_WEBSOCKET_EXPERIMENT_RUNNER_H_
