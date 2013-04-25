// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_HELPERS_H_
#define MEDIA_BASE_TEST_HELPERS_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/size.h"

namespace base {
class MessageLoop;
}

namespace media {

// Return a callback that expects to be run once.
base::Closure NewExpectedClosure();
PipelineStatusCB NewExpectedStatusCB(PipelineStatus status);

// Helper class for running a message loop until a callback has run. Useful for
// testing classes that run on more than a single thread.
//
// Events are intended for single use and cannot be reset.
class WaitableMessageLoopEvent {
 public:
  WaitableMessageLoopEvent();
  ~WaitableMessageLoopEvent();

  // Returns a thread-safe closure that will signal |this| when executed.
  base::Closure GetClosure();
  PipelineStatusCB GetPipelineStatusCB();

  // Runs the current message loop until |this| has been signaled.
  //
  // Fails the test if the timeout is reached.
  void RunAndWait();

  // Runs the current message loop until |this| has been signaled and asserts
  // that the |expected| status was received.
  //
  // Fails the test if the timeout is reached.
  void RunAndWaitForStatus(PipelineStatus expected);

 private:
  void OnCallback(PipelineStatus status);
  void OnTimeout();

  base::MessageLoop* message_loop_;
  bool signaled_;
  PipelineStatus status_;

  DISALLOW_COPY_AND_ASSIGN(WaitableMessageLoopEvent);
};

// Provides pre-canned VideoDecoderConfig. These types are used for tests that
// don't care about detailed parameters of the config.
class TestVideoConfig {
 public:
  // Returns a configuration that is invalid.
  static VideoDecoderConfig Invalid();

  static VideoDecoderConfig Normal();
  static VideoDecoderConfig NormalEncrypted();

  // Returns a configuration that is larger in dimensions than Normal().
  static VideoDecoderConfig Large();
  static VideoDecoderConfig LargeEncrypted();

  // Returns coded size for Normal and Large config.
  static gfx::Size NormalCodedSize();
  static gfx::Size LargeCodedSize();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TestVideoConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_TEST_HELPERS_H_
