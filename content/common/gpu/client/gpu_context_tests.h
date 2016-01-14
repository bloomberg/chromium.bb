// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are run twice:
// Once in a gpu test with an in-process WebGraphicsContext3D.
// Once in a browsertest with a gpu-process WebGraphicsContext3D.

#include "base/bind.h"
#include "base/run_loop.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace {

class SignalTest : public ContextTestBase {
 public:
  static void RunOnlyOnce(base::Closure cb, int* tmp) {
    CHECK_EQ(*tmp, 0);
    ++*tmp;
    cb.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void TestSignalSyncToken(const gpu::SyncToken& sync_token) {
    base::RunLoop run_loop;
    context_support_->SignalSyncToken(sync_token, run_loop.QuitClosure());
    run_loop.Run();
  }

  // These tests should time out if the callback doesn't get called.
  void TestSignalQuery(blink::WebGLId query) {
    base::RunLoop run_loop;
    context_support_->SignalQuery(
        query,
        base::Bind(
            &RunOnlyOnce, run_loop.QuitClosure(), base::Owned(new int(0))));
    run_loop.Run();
  }
};

CONTEXT_TEST_F(SignalTest, BasicSignalSyncTokenTest) {
  if (!context_)
    return;

  const blink::WGC3Duint64 fence_sync = context_->insertFenceSyncCHROMIUM();
  context_->shallowFlushCHROMIUM();

  gpu::SyncToken sync_token;
  ASSERT_TRUE(context_->genSyncTokenCHROMIUM(fence_sync, sync_token.GetData()));

  TestSignalSyncToken(sync_token);
};

CONTEXT_TEST_F(SignalTest, EmptySignalSyncTokenTest) {
  if (!context_)
    return;

  // Signalling something that doesn't exist should run the callback
  // immediately.
  gpu::SyncToken sync_token;
  TestSignalSyncToken(sync_token);
};

CONTEXT_TEST_F(SignalTest, InvalidSignalSyncTokenTest) {
  if (!context_)
    return;

  // Signalling something that doesn't exist should run the callback
  // immediately.
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            0,
                            1297824234,
                            9123743439);
  TestSignalSyncToken(sync_token);
};

CONTEXT_TEST_F(SignalTest, BasicSignalQueryTest) {
  if (!context_)
    return;

  unsigned query = context_->createQueryEXT();
  context_->beginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query);
  context_->finish();
  context_->endQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  TestSignalQuery(query);
  context_->deleteQueryEXT(query);
};

CONTEXT_TEST_F(SignalTest, SignalQueryUnboundTest) {
  if (!context_)
    return;

  blink::WebGLId query = context_->createQueryEXT();
  TestSignalQuery(query);
  context_->deleteQueryEXT(query);
};

CONTEXT_TEST_F(SignalTest, InvalidSignalQueryUnboundTest) {
  if (!context_)
    return;

  // Signalling something that doesn't exist should run the callback
  // immediately.
  TestSignalQuery(928729087);
  TestSignalQuery(928729086);
  TestSignalQuery(928729085);
  TestSignalQuery(928729083);
  TestSignalQuery(928729082);
  TestSignalQuery(928729081);
};

};
