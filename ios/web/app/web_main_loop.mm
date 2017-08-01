// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/app/web_main_loop.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/process_metrics.h"
#include "base/system_monitor/system_monitor.h"
#include "base/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#import "ios/web/net/cookie_notification_bridge.h"
#include "ios/web/public/app/web_main_parts.h"
#include "ios/web/public/global_state/ios_global_state.h"
#import "ios/web/public/web_client.h"
#include "ios/web/service_manager_context.h"
#include "ios/web/web_thread_impl.h"
#include "ios/web/webui/url_data_manager_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// The currently-running WebMainLoop.  There can be one or zero.
// TODO(rohitrao): Desktop uses this to implement
// ImmediateShutdownAndExitProcess.  If we don't need that functionality, we can
// remove this.
WebMainLoop* g_current_web_main_loop = nullptr;

WebMainLoop::WebMainLoop()
    : result_code_(0),
      created_threads_(false),
      destroy_message_loop_(base::Bind(&ios_global_state::DestroyMessageLoop)),
      destroy_network_change_notifier_(
          base::Bind(&ios_global_state::DestroyNetworkChangeNotifier)) {
  DCHECK(!g_current_web_main_loop);
  g_current_web_main_loop = this;
}

WebMainLoop::~WebMainLoop() {
  DCHECK_EQ(this, g_current_web_main_loop);
  g_current_web_main_loop = nullptr;
}

void WebMainLoop::Init() {
  parts_ = web::GetWebClient()->CreateWebMainParts();
}

void WebMainLoop::EarlyInitialization() {
  if (parts_) {
    parts_->PreEarlyInitialization();
    parts_->PostEarlyInitialization();
  }
}

void WebMainLoop::MainMessageLoopStart() {
  if (parts_) {
    parts_->PreMainMessageLoopStart();
  }

  ios_global_state::BuildMessageLoop();

  InitializeMainThread();

#if 0
  // TODO(crbug.com/228014): SystemMonitor is not working properly on iOS.
  system_monitor_.reset(new base::SystemMonitor);
#endif
  // TODO(rohitrao): Do we need PowerMonitor on iOS, or can we get rid of it?
  std::unique_ptr<base::PowerMonitorSource> power_monitor_source(
      new base::PowerMonitorDeviceSource());
  power_monitor_.reset(new base::PowerMonitor(std::move(power_monitor_source)));

  ios_global_state::CreateNetworkChangeNotifier();

  if (parts_) {
    parts_->PostMainMessageLoopStart();
  }
}

void WebMainLoop::CreateStartupTasks(
    TaskSchedulerInitParamsCallback init_params_callback) {
  int result = 0;
  result = PreCreateThreads();
  if (result > 0)
    return;

  result = CreateThreads(std::move(init_params_callback));
  if (result > 0)
    return;

  result = WebThreadsStarted();
  if (result > 0)
    return;

  result = PreMainMessageLoopRun();
  if (result > 0)
    return;
}

int WebMainLoop::PreCreateThreads() {
  if (parts_) {
    parts_->PreCreateThreads();
  }

  return result_code_;
}

int WebMainLoop::CreateThreads(
    TaskSchedulerInitParamsCallback init_params_callback) {
  std::unique_ptr<base::TaskScheduler::InitParams> init_params;
  if (!init_params_callback.is_null()) {
    init_params = std::move(init_params_callback).Run();
  }
  ios_global_state::StartTaskScheduler(init_params.get());

  base::SequencedWorkerPool::EnableWithRedirectionToTaskSchedulerForProcess();

  base::Thread::Options io_message_loop_options;
  io_message_loop_options.message_loop_type = base::MessageLoop::TYPE_IO;

  // Start threads in the order they occur in the WebThread::ID
  // enumeration, except for WebThread::UI which is the main
  // thread.
  //
  // Must be size_t so we can increment it.
  for (size_t thread_id = WebThread::UI + 1; thread_id < WebThread::ID_COUNT;
       ++thread_id) {
    std::unique_ptr<WebThreadImpl>* thread_to_start = nullptr;
    base::Thread::Options options;

    switch (thread_id) {
      // TODO(rohitrao): We probably do not need all of these threads.  Remove
      // the ones that serve no purpose.  http://crbug.com/365909
      case WebThread::DB:
        thread_to_start = &db_thread_;
        options.timer_slack = base::TIMER_SLACK_MAXIMUM;
        break;
      case WebThread::FILE:
        thread_to_start = &file_thread_;
        options = io_message_loop_options;
        options.timer_slack = base::TIMER_SLACK_MAXIMUM;
        break;
      case WebThread::IO:
        thread_to_start = &io_thread_;
        options = io_message_loop_options;
        break;
      case WebThread::UI:
      case WebThread::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }

    WebThread::ID id = static_cast<WebThread::ID>(thread_id);

    if (thread_to_start) {
      (*thread_to_start).reset(new WebThreadImpl(id));
      (*thread_to_start)->StartWithOptions(options);
    } else {
      NOTREACHED();
    }
  }
  created_threads_ = true;
  return result_code_;
}

int WebMainLoop::PreMainMessageLoopRun() {
  if (parts_) {
    parts_->PreMainMessageLoopRun();
  }

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow disk IO from the UI thread.
  base::ThreadRestrictions::SetIOAllowed(false);
  base::ThreadRestrictions::DisallowWaiting();
  return result_code_;
}

void WebMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  WebThread::PostTask(
      WebThread::IO, FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 true));

  if (parts_) {
    parts_->PostMainMessageLoopRun();
  }

  service_manager_context_.reset();

  // Must be size_t so we can subtract from it.
  for (size_t thread_id = WebThread::ID_COUNT - 1;
       thread_id >= (WebThread::UI + 1); --thread_id) {
    // Find the thread object we want to stop. Looping over all valid
    // WebThread IDs and DCHECKing on a missing case in the switch
    // statement helps avoid a mismatch between this code and the
    // WebThread::ID enumeration.
    //
    // The destruction order is the reverse order of occurrence in the
    // WebThread::ID list. The rationale for the order is as
    // follows (need to be filled in a bit):
    //
    //
    // - (Not sure why DB stops last.)
    switch (thread_id) {
      case WebThread::DB:
        db_thread_.reset();
        break;
      case WebThread::FILE:
        file_thread_.reset();
        break;
      case WebThread::IO:
        io_thread_.reset();
        break;
      case WebThread::UI:
      case WebThread::ID_COUNT:
      default:
        NOTREACHED();
        break;
    }
  }

  // Close the blocking I/O pool after the other threads. Other threads such
  // as the I/O thread may need to schedule work like closing files or flushing
  // data during shutdown, so the blocking pool needs to be available. There
  // may also be slow operations pending that will block shutdown, so closing
  // it here (which will block until required operations are complete) gives
  // more head start for those operations to finish.
  WebThreadImpl::ShutdownThreadPool();
  base::TaskScheduler::GetInstance()->Shutdown();

  URLDataManagerIOS::DeleteDataSources();

  if (parts_) {
    parts_->PostDestroyThreads();
  }
}

void WebMainLoop::InitializeMainThread() {
  base::PlatformThread::SetName("CrWebMain");

  // Register the main thread by instantiating it, but don't call any methods.
  main_thread_.reset(
      new WebThreadImpl(WebThread::UI, base::MessageLoop::current()));
}

int WebMainLoop::WebThreadsStarted() {
  cookie_notification_bridge_.reset(new CookieNotificationBridge);
  service_manager_context_ = base::MakeUnique<ServiceManagerContext>();
  return result_code_;
}

}  // namespace web
