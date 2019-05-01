// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THREAD_POOL_UTIL_VARIATIONS_UTIL_H_
#define COMPONENTS_THREAD_POOL_UTIL_VARIATIONS_UTIL_H_

#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool/thread_pool.h"

namespace thread_pool_util {

extern const base::Feature kBrowserThreadPoolInitParams;
extern const base::Feature kRendererThreadPoolInitParams;

// Builds a ThreadPool::InitParams from variations params that are prefixed
// for |feature|. Returns nullptr on failure.
//
// TODO(fdoray): Move this to the anonymous namespace in the .cc file.
// https://crbug.com/810049
std::unique_ptr<base::ThreadPool::InitParams> GetThreadPoolInitParams(
    const base::Feature& feature);

// Builds a ThreadPool::InitParams to use in the browser process from
// variation params in the BrowserThreadPool field trial.
std::unique_ptr<base::ThreadPool::InitParams>
GetThreadPoolInitParamsForBrowser();

// Builds a ThreadPool::InitParams to use in renderer processes from
// variation params in the RendererThreadPool field trial.
std::unique_ptr<base::ThreadPool::InitParams>
GetThreadPoolInitParamsForRenderer();

}  // namespace thread_pool_util

#endif  // COMPONENTS_THREAD_POOL_UTIL_VARIATIONS_UTIL_H_
