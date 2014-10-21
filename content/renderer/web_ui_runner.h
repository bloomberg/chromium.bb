// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_RUNNER_H_
#define CONTENT_RENDERER_WEB_UI_RUNNER_H_

#include "gin/runner.h"

namespace blink {
class WebFrame;
}

namespace content {

// Implementation of gin::Runner that forwards Runner functions to WebFrame.
class WebUIRunner : public gin::Runner {
 public:
  // Does not take ownership of ContextHolder.
  WebUIRunner(blink::WebFrame* frame, gin::ContextHolder* context_holder);
  ~WebUIRunner() override;

  // Runner overrides:
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Handle<v8::Value> Call(v8::Handle<v8::Function> function,
                             v8::Handle<v8::Value> receiver,
                             int argc,
                             v8::Handle<v8::Value> argv[]) override;
  gin::ContextHolder* GetContextHolder() override;

 private:
  // Frame to execute script in.
  blink::WebFrame* frame_;

  // Created by blink bindings to V8.
  gin::ContextHolder* context_holder_;

  DISALLOW_COPY_AND_ASSIGN(WebUIRunner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_RUNNER_H_
