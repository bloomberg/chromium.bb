// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/fuzzer_support.h"

#include <string>

#include "base/feature_list.h"
#include "base/i18n/icu_util.h"
#include "content/common/navigation_params.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/test_render_frame.h"
#include "gin/v8_initializer.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

namespace content {

void RenderViewTestAdapter::SetUp() {
  RenderViewTest::SetUp();
}

Env::Env() {
  base::CommandLine::Init(0, nullptr);
  base::FeatureList::InitializeInstance(std::string(), std::string());
  base::i18n::InitializeICU();

  blink::WebRuntimeFeatures::enableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::enableTestOnlyFeatures(true);

  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());

  adapter.reset(new RenderViewTestAdapter());
  adapter->SetUp();
}

Env::~Env() {
  LOG(FATAL) << "NOT SUPPORTED";
}
}  // namespace content
