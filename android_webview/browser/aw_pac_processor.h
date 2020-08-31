// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_

#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/log/net_log_with_source.h"
#include "services/proxy_resolver/proxy_host_resolver.h"
#include "services/proxy_resolver/proxy_resolver_v8_tracing.h"

namespace android_webview {

class AwPacProcessor {
 public:
  static AwPacProcessor* Get();
  jboolean SetProxyScript(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          const base::android::JavaParamRef<jstring>& jscript);
  bool SetProxyScript(std::string script);
  base::android::ScopedJavaLocalRef<jstring> MakeProxyRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl);
  std::string MakeProxyRequest(std::string url);
  proxy_resolver::ProxyHostResolver* host_resolver() {
    return host_resolver_.get();
  }

 private:
  AwPacProcessor();
  AwPacProcessor(const AwPacProcessor&) = delete;
  AwPacProcessor& operator=(const AwPacProcessor&) = delete;
  ~AwPacProcessor();
  void SetProxyScriptNative(
      std::unique_ptr<net::ProxyResolverFactory::Request>* request,
      const std::string& script,
      net::CompletionOnceCallback complete);
  void MakeProxyRequestNative(
      std::unique_ptr<net::ProxyResolver::Request>* request,
      const std::string& url,
      net::ProxyInfo* proxy_info,
      net::CompletionOnceCallback complete);
  std::unique_ptr<proxy_resolver::ProxyResolverV8TracingFactory>
      proxy_resolver_factory_;
  std::unique_ptr<proxy_resolver::ProxyResolverV8Tracing> proxy_resolver_;
  std::unique_ptr<proxy_resolver::ProxyHostResolver> host_resolver_;

  base::Thread thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  friend class base::NoDestructor<AwPacProcessor>;

  friend class Job;
  friend class SetProxyScriptJob;
  friend class MakeProxyRequestJob;
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_PAC_PROCESSOR_H_