// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_

#include <jni.h>

#include <queue>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class NetLogLogger;
class URLRequestContext;
class ProxyConfigService;
}  // namespace net

namespace cronet {

struct URLRequestContextConfig;

bool CronetUrlRequestContextAdapterRegisterJni(JNIEnv* env);

// Adapter between Java CronetUrlRequestContext and net::URLRequestContext.
class CronetURLRequestContextAdapter {
 public:
  explicit CronetURLRequestContextAdapter(
      scoped_ptr<URLRequestContextConfig> context_config);

  ~CronetURLRequestContextAdapter();

  // Called on main Java thread to initialize URLRequestContext.
  void InitRequestContextOnMainThread(JNIEnv* env, jobject jcaller);

  // Releases all resources for the request context and deletes the object.
  // Blocks until network thread is destroyed after running all pending tasks.
  void Destroy(JNIEnv* env, jobject jcaller);

  // Posts a task that might depend on the context being initialized
  // to the network thread.
  void PostTaskToNetworkThread(const tracked_objects::Location& posted_from,
                               const base::Closure& callback);

  bool IsOnNetworkThread() const;

  net::URLRequestContext* GetURLRequestContext();

  void StartNetLogToFile(JNIEnv* env, jobject jcaller, jstring jfile_name);

  void StopNetLog(JNIEnv* env, jobject jcaller);

  // Default net::LOAD flags used to create requests.
  int default_load_flags() const { return default_load_flags_; }

  // Called on main Java thread to initialize URLRequestContext.
  void InitRequestContextOnMainThread();

 private:
  // Initializes |context_| on the Network thread.
  void InitializeOnNetworkThread(scoped_ptr<URLRequestContextConfig> config,
                                 const base::android::ScopedJavaGlobalRef<
                                     jobject>& jcronet_url_request_context);

  // Runs a task that might depend on the context being initialized.
  // This method should only be run on the network thread.
  void RunTaskAfterContextInitOnNetworkThread(
      const base::Closure& task_to_run_after_context_init);

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner() const;

  void StartNetLogToFileOnNetworkThread(const std::string& file_name);

  void StopNetLogOnNetworkThread();

  // Network thread is owned by |this|, but is destroyed from java thread.
  base::Thread* network_thread_;
  // |net_log_logger_| and |context_| should only be accessed on network thread.
  scoped_ptr<net::NetLogLogger> net_log_logger_;
  scoped_ptr<net::URLRequestContext> context_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;

  // Context config is only valid untng context is initialized.
  scoped_ptr<URLRequestContextConfig> context_config_;

  // A queue of tasks that need to be run after context has been initialized.
  std::queue<base::Closure> tasks_waiting_for_context_;
  bool is_context_initialized_;
  int default_load_flags_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestContextAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_CONTEXT_ADAPTER_H_
