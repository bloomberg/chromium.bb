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
#include "native_client/src/debug_server/gdb_rsp/host.h"
#include "native_client/src/debug_server/gdb_rsp/session_mock.h"
#include "native_client/src/debug_server/gdb_rsp/test.h"

using gdb_rsp::Abi;
using gdb_rsp::Host;
using gdb_rsp::SessionMock;

int VerifyProp(Host* host, const char *key, const char *val) {
  std::string out;
  if (!host->ReadProperty(key, &out)) {
    printf("Could not find key: %s.\n", key);
    return 1;
  }

  if (out != val) {
    printf("Property mismatch\n\tVALUE:%s\n\tEXPECTED:%s\n",
           out.data(), val);
    return 1;
  }

  return 0;
}

int TestHost() {
  int errs = 0;
  SessionMock *ses = gdb_rsp::GetGoldenSessionMock(false, false);
  const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();

  Host *host = new Host(ses);
  std::string val;
  if (!host->Init()) {
    printf("Failed to init host.\n");
    return 1;
  }

  // Verify that the Init/Update transactions were
  // correctly applied.
  if (!host->HasProperty("qXfer:features:read")) {
    printf("Missing property: qXfer:features:read\n");
    errs++;
  }

  errs += VerifyProp(host, "PacketSize", "7cf");
  errs += VerifyProp(host, "qXfer:libraries:read", "true");
  Host::Thread* thread = host->GetThread(0x1234);
  if (NULL == thread) {
    printf("Failed to find expected thead 1234.\n");
    errs++;
  } else {
    for (uint32_t a = 0; a < abi->GetRegisterCount(); a++) {
      const Abi::RegDef* def = abi->GetRegisterDef(a);
      uint64_t val;
      thread->GetRegister(a, &val);
      if (static_cast<uint32_t>(val) != a) {
        errs++;
        printf("Register %s(%d) failed to match expected value: %x",
               def->name_, a, static_cast<int>(val));
        break;
      }
    }
  }

  if (host->GetStatus() != Host::HS_STOPPED) {
    printf("We expect to be stopped at this point.\n");
    errs++;
  }

  if (host->GetSignal() != 5) {
    printf("We expect to see signal 5.\n");
    errs++;
  }

  if (ses->PeekAction() != SessionMock::DISCONNECT) {
    printf("We did not consume all the actions.\n");
    errs++;
  }

  // Pop off the DC, and continue processing.
  ses->GetAction();

  // Verify we are now in a running state
  ses->AddAction(SessionMock::SEND, "c");
  host->Continue();

  // We should be running, and timing out on wait
  if (host->GetStatus() != Host::HS_RUNNING) {
    printf("We expect to be running at this point.\n");
    errs++;
  }
  if (host->WaitForBreak()) {
    printf("We should have timed out.\n");
    errs++;
  }

  // Insert a signal 11 for us to wait on
  ses->AddAction(SessionMock::RECV, "S11");
  AddMockUpdate(ses, 0x11);
  if (!host->WaitForBreak()) {
    printf("We should have gotten a signal.\n");
    errs++;
  }
  if (host->GetStatus() != Host::HS_STOPPED) {
    printf("We expect to be stopped at this point.\n");
    errs++;
  }
  if (host->GetSignal() != 0x11) {
    printf("We expect to see signal 11.\n");
    errs++;
  }

  delete host;
  delete ses;
  return errs;
}

