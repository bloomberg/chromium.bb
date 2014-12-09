// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WorkerFrame API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_CONSTANTS_H_

namespace worker_frame {

// API namespace for the *embedder*. The embedder and guest use different APIs.
extern const char kEmbedderAPINamespace[];

extern const char kURL[];

}  // namespace worker_frame

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_WORKER_FRAME_WORKER_FRAME_CONSTANTS_H_
