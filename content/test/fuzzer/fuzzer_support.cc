// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fuzzer/fuzzer_support.h"

#include <string>

#include "base/feature_list.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "gin/v8_initializer.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"

namespace content {

void RenderViewTestAdapter::SetUp() {
  RenderViewTest::SetUp();
}

Env::Env() {
  base::CommandLine::Init(0, nullptr);
  base::FeatureList::InitializeInstance(std::string(), std::string());
  base::i18n::InitializeICU();

  blink::WebRuntimeFeatures::EnableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::EnableTestOnlyFeatures(true);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
  gin::V8Initializer::LoadV8Natives();
#endif
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::IsolateHolder::kStableV8Extras,
                                 gin::ArrayBufferAllocator::SharedInstance());

  adapter = std::make_unique<RenderViewTestAdapter>();
  adapter->SetUp();
}

Env::~Env() {
  LOG(FATAL) << "NOT SUPPORTED";
}

}  // namespace content
