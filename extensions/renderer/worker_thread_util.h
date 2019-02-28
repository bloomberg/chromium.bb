// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WORKER_THREAD_UTIL_H_
#define EXTENSIONS_RENDERER_WORKER_THREAD_UTIL_H_

namespace extensions {
namespace worker_thread_util {

// Returns true if the current thread is a worker thread.
bool IsWorkerThread();

}  // namespace worker_thread_util
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_WORKER_THREAD_UTIL_H_
