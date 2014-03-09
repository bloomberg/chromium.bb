// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_

namespace content {
struct ContentMainParams;

// This class is responsible for content initialization, running and shutdown.
class ContentMainRunner {
 public:
  virtual ~ContentMainRunner() {}

  // Create a new ContentMainRunner object.
  static ContentMainRunner* Create();

  // Initialize all necessary content state.
  virtual int Initialize(const ContentMainParams& params) = 0;

  // Perform the default run logic.
  virtual int Run() = 0;

  // Shut down the content state.
  virtual void Shutdown() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
