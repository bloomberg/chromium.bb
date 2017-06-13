// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/single_log_source_factory.h"

#include "base/memory/ptr_util.h"

namespace extensions {

namespace {

using system_logs::SingleLogSource;

SingleLogSourceFactory::CreateCallback* g_callback = nullptr;

}  // namespace

// static
std::unique_ptr<SingleLogSource> SingleLogSourceFactory::CreateSingleLogSource(
    SingleLogSource::SupportedSource type) {
  if (g_callback)
    return g_callback->Run(type);
  return base::MakeUnique<SingleLogSource>(type);
}

// static
void SingleLogSourceFactory::SetForTesting(
    SingleLogSourceFactory::CreateCallback* callback) {
  g_callback = callback;
}

}  // namespace extensions
