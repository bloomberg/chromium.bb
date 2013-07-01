// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are run twice:
// Once in a gpu test with an in-process WebGraphicsContext3D.
// Once in a browsertest with a gpu-process WebGraphicsContext3D.

#include "base/bind.h"
#include "base/run_loop.h"
#include "cc/resources/sync_point_helper.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace {

class SignalTest : public ContextTestBase {
 public:
  static void RunOnlyOnce(base::Closure cb, int *tmp) {
    CHECK_EQ(*tmp, 0);
    ++*tmp;
    cb.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void testSignalSyncPoint(unsigned sync_point) {
    base::RunLoop run_loop;
    cc::SyncPointHelper::SignalSyncPoint(
        context_.get(), sync_point, run_loop.QuitClosure());
    run_loop.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void testSignalQuery(WebKit::WebGLId query) {
    base::RunLoop run_loop;
    cc::SyncPointHelper::SignalQuery(
        context_.get(), query,
        base::Bind(&RunOnlyOnce, run_loop.QuitClosure(),
                   base::Owned(new int(0))));
    run_loop.Run();
  }
};

CONTEXT_TEST_F(SignalTest, BasicSignalSyncPointTest) {
  testSignalSyncPoint(context_->insertSyncPoint());
};

CONTEXT_TEST_F(SignalTest, InvalidSignalSyncPointTest) {
  // Signalling something that doesn't exist should run the callback
  // immediately.
  testSignalSyncPoint(1297824234);
};

CONTEXT_TEST_F(SignalTest, BasicSignalQueryTest) {
  unsigned query = context_->createQueryEXT();
  context_->beginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);
  context_->finish();
  context_->endQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  testSignalQuery(query);
  context_->deleteQueryEXT(query);
};

CONTEXT_TEST_F(SignalTest, SignalQueryUnboundTest) {
  WebKit::WebGLId query = context_->createQueryEXT();
  testSignalQuery(query);
  context_->deleteQueryEXT(query);
};

CONTEXT_TEST_F(SignalTest, InvalidSignalQueryUnboundTest) {
  // Signalling something that doesn't exist should run the callback
  // immediately.
  testSignalQuery(928729087);
  testSignalQuery(928729086);
  testSignalQuery(928729085);
  testSignalQuery(928729083);
  testSignalQuery(928729082);
  testSignalQuery(928729081);
};

};
