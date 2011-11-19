/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <sstream>
#include <vector>

#include "native_client/src/debug_server/gdb_rsp/abi.h"
#include "native_client/src/debug_server/gdb_rsp/session_mock.h"
#include "native_client/src/debug_server/gdb_rsp/target.h"
#include "native_client/src/debug_server/gdb_rsp/test.h"

using gdb_rsp::Abi;
using gdb_rsp::SessionMock;
using gdb_rsp::Target;

int TestTarget() {
  int errs = 0;

  SessionMock *ses = gdb_rsp::GetGoldenSessionMock(true, false);
  const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();

  Target *target = new Target(NULL);
  if (!target->Init()) {
    printf("Failed to INIT target.\n");
    return 1;
  }

  // Create a pseudo thread with registers set to their index
  port::IThread *thread = port::IThread::Acquire(0x1234, true);
  for (uint32_t a = 0; a < abi->GetRegisterCount(); a++) {
    const Abi::RegDef* def = abi->GetRegisterDef(a);
    uint64_t val = static_cast<uint64_t>(a);
    thread->SetRegister(a, &val, def->bytes_);
  }
  target->TrackThread(thread);

  // Pretend we just got a signal on that thread
  target->Signal(thread->GetId(), 5, false);

  // Run the session to completion.
  target->Run(ses);

  if (ses->PeekAction() != SessionMock::DISCONNECT) {
    printf("We did not consume all the actions.\n");
    errs++;
  }

  delete target;
  delete ses;
  return errs;
}

