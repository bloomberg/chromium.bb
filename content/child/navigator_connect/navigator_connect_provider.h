// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_PROVIDER_H_
#define CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_PROVIDER_H_

#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebNavigatorConnectProvider.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebMessagePortChannel;
class WebString;
}

namespace IPC {
class Message;
}

namespace content {
class ThreadSafeSender;
struct TransferredMessagePort;

// Main entry point for the navigator.connect API in a child process. This
// implements the blink API and passes connect calls on to the browser process.
// NavigatorConnectDispatcher receives IPCs from the browser process and passes
// them on to the correct thread specific instance of this class.
// The ThreadSpecificInstance method will return an instance of this class for
// the current thread, or create one if no instance exists yet for this thread.
// This class deletes itself if the worker thread it belongs to quits.
class NavigatorConnectProvider : public blink::WebNavigatorConnectProvider,
                                 public WorkerTaskRunner::Observer {
 public:
  NavigatorConnectProvider(
      ThreadSafeSender* thread_safe_sender,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_loop);
  ~NavigatorConnectProvider();

  // WebNavigatorConnectProvider implementation.
  virtual void connect(const blink::WebURL& target_url,
                       const blink::WebString& origin,
                       blink::WebNavigatorConnectPortCallbacks* callbacks);

  void OnMessageReceived(const IPC::Message& msg);

  // |thread_safe_sender| and |main_loop| need to be passed in because if the
  // call leads to construction they will be needed.
  static NavigatorConnectProvider* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_loop);

 private:
  void OnConnectResult(int thread_id,
                       int request_id,
                       const TransferredMessagePort& message_port,
                       int message_port_route_id,
                       bool allow_connect);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_loop_;
  IDMap<blink::WebNavigatorConnectPortCallbacks, IDMapOwnPointer> requests_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorConnectProvider);
};

}  // namespace content

#endif  // CONTENT_CHILD_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_PROVIDER_H_
