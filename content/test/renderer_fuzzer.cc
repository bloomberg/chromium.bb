// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for content/renderer

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/feature_list.h"
#include "content/common/navigation_params.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/test_render_frame.h"
#include "gin/v8_initializer.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

namespace content {

class FuzzerRenderTest : public RenderViewTest {
 public:
  FuzzerRenderTest() : RenderViewTest() {}
  void TestBody() override {}

  void SetUp() override { RenderViewTest::SetUp(); }

  TestRenderFrame* frame() {
    return static_cast<TestRenderFrame*>(view_->GetMainRenderFrame());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FuzzerRenderTest);
};

struct Env {
  Env() {
    base::CommandLine::Init(0, nullptr);
    base::FeatureList::InitializeInstance(std::string(), std::string());

    blink::WebRuntimeFeatures::enableExperimentalFeatures(true);
    blink::WebRuntimeFeatures::enableTestOnlyFeatures(true);
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();

    test.reset(new FuzzerRenderTest());
    test->SetUp();
  }

  TestRenderFrame* frame() { return test->frame(); }

  std::unique_ptr<FuzzerRenderTest> test;

 private:
  DISALLOW_COPY_AND_ASSIGN(Env);
};

static Env* env = new Env();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string input(reinterpret_cast<const char*>(data), size);

  CommonNavigationParams common_params;
  common_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  common_params.url = GURL("data:text/html," + input);
  env->frame()->Navigate(common_params, StartNavigationParams(),
                         RequestNavigationParams());

  return 0;
}

}  // namespace content
