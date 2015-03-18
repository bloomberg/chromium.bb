// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBSCHEDULER_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBSCHEDULER_IMPL_H_

#include "third_party/WebKit/public/platform/WebScheduler.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace html_viewer {

class WebSchedulerImpl : public blink::WebScheduler {
 public:
  explicit WebSchedulerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~WebSchedulerImpl() override;

 private:
  // blink::WebScheduler overrides.
  virtual void postIdleTask(const blink::WebTraceLocation& location,
                    blink::WebScheduler::IdleTask* task);
  virtual void postLoadingTask(const blink::WebTraceLocation& location,
                       blink::WebThread::Task* task);
  virtual void postTimerTask(const blink::WebTraceLocation& location,
                             blink::WebThread::Task* task,
                             long long delayMs);
  virtual void shutdown();

  static void RunIdleTask(scoped_ptr<blink::WebScheduler::IdleTask> task);
  static void RunTask(scoped_ptr<blink::WebThread::Task> task);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebSchedulerImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBSCHEDULER_IMPL_H_
