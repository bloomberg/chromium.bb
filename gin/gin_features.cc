// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin_features.h"

namespace features {

// Enables optimization of JavaScript in V8.
const base::Feature kV8OptimizeJavascript{"V8OptimizeJavascript",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables flushing of JS bytecode in V8.
const base::Feature kV8FlushBytecode{"V8FlushBytecode",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables memory reducer for small heaps in V8.
const base::Feature kV8MemoryReducerForSmallHeaps{
    "V8MemoryReducerForSmallHeaps", base::FEATURE_ENABLED_BY_DEFAULT};

// Increase V8 heap size to 4GB if the physical memory is bigger than 16 GB.
const base::Feature kV8HugeMaxOldGenerationSize{
    "V8HugeMaxOldGenerationSize", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
