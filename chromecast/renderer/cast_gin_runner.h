// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_GIN_RUNNER_H_
#define CHROMECAST_RENDERER_CAST_GIN_RUNNER_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "gin/runner.h"

namespace blink {
class WebFrame;
}

namespace content {
class RenderFrame;
}

namespace gin {
class PerContextData;
}

namespace chromecast {
namespace shell {

// Implementation of gin::Runner that forwards Runner functions to WebFrame.
// This class is lazily created per RenderFrame with the Get function and it's
// lifetime is managed by the gin::PerContextData associated with the frame.
class CastGinRunner : public gin::Runner, public base::SupportsUserData::Data {
 public:
  // Gets or creates the CastGinRunner for this RenderFrame
  static CastGinRunner* Get(content::RenderFrame* render_frame);
  ~CastGinRunner() override;

  // gin:Runner implementation:
  void Run(const std::string& source,
           const std::string& resource_name) override;
  v8::Local<v8::Value> Call(v8::Local<v8::Function> function,
                            v8::Local<v8::Value> receiver,
                            int argc,
                            v8::Local<v8::Value> argv[]) override;
  gin::ContextHolder* GetContextHolder() override;

 private:
  CastGinRunner(blink::WebFrame* frame, gin::PerContextData* context_data);

  // Frame to execute script in.
  blink::WebFrame* const frame_;

  // Created by blink bindings to V8.
  gin::ContextHolder* const context_holder_;

  DISALLOW_COPY_AND_ASSIGN(CastGinRunner);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_GIN_RUNNER_H_
