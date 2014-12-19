// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WorkerFrame API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_SURFACE_WORKER_SURFACE_WORKER_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_SURFACE_WORKER_SURFACE_WORKER_CONSTANTS_H_

namespace surface_worker {

// API namespace for the *embedder*. The embedder and guest use different APIs.
extern const char kEmbedderAPINamespace[];

extern const char kURL[];

}  // namespace surface_worker

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_SURFACE_WORKER_SURFACE_WORKER_CONSTANTS_H_
