// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net_benchmarking_extension.h"

#include "chrome/common/benchmarking_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "v8/include/v8.h"

using WebKit::WebCache;

const char kNetBenchmarkingExtensionName[] = "v8/NetBenchmarking";

namespace extensions_v8 {

class NetBenchmarkingWrapper : public v8::Extension {
 public:
  NetBenchmarkingWrapper() :
      v8::Extension(kNetBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.benchmarking) == 'undefined') {"
        "  chrome.benchmarking = {};"
        "};"
        "chrome.benchmarking.clearCache = function() {"
        "  native function ClearCache();"
        "  ClearCache();"
        "};"
        "chrome.benchmarking.clearHostResolverCache = function() {"
        "  native function ClearHostResolverCache();"
        "  ClearHostResolverCache();"
        "};"
        "chrome.benchmarking.clearPredictorCache = function() {"
        "  native function ClearPredictorCache();"
        "  ClearPredictorCache();"
        "};"
        "chrome.benchmarking.closeConnections = function() {"
        "  native function CloseConnections();"
        "  CloseConnections();"
        "};"
        "chrome.benchmarking.enableSpdy = function(name) {"
        "  native function EnableSpdy();"
        "  EnableSpdy(name);"
        "};"
        ) {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("ClearCache"))) {
      return v8::FunctionTemplate::New(ClearCache);
    } else if (name->Equals(v8::String::New("ClearHostResolverCache"))) {
      return v8::FunctionTemplate::New(ClearHostResolverCache);
    } else if (name->Equals(v8::String::New("ClearPredictorCache"))) {
      return v8::FunctionTemplate::New(ClearPredictorCache);
    } else if (name->Equals(v8::String::New("EnableSpdy"))) {
      return v8::FunctionTemplate::New(EnableSpdy);
    } else if (name->Equals(v8::String::New("CloseConnections"))) {
      return v8::FunctionTemplate::New(CloseConnections);
    }

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void ClearCache(const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(new ChromeViewHostMsg_ClearCache(&rv));
    WebCache::clear();
  }

  static void ClearHostResolverCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_ClearHostResolverCache(&rv));
  }

  static void ClearPredictorCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    int rv;
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_ClearPredictorCache(&rv));
  }

  static void CloseConnections(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    content::RenderThread::Get()->Send(
        new ChromeViewHostMsg_CloseCurrentConnections());
  }

  static void EnableSpdy(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!args.Length() || !args[0]->IsBoolean())
      return;

    content::RenderThread::Get()->Send(new ChromeViewHostMsg_EnableSpdy(
        args[0]->BooleanValue()));
  }
};

v8::Extension* NetBenchmarkingExtension::Get() {
  return new NetBenchmarkingWrapper();
}

}  // namespace extensions_v8
