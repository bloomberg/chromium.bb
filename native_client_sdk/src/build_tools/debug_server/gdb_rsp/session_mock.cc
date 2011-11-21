/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <sstream>

#include "native_client/src/debug_server/gdb_rsp/abi.h"
#include "native_client/src/debug_server/gdb_rsp/session.h"
#include "native_client/src/debug_server/gdb_rsp/session_mock.h"
#include "native_client/src/debug_server/port/platform.h"
#include "native_client/src/debug_server/port/thread.h"
#include "native_client/src/debug_server/port/transport.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

namespace gdb_rsp {

SessionMock::SessionMock(bool target) : Session() {
  target_ = target;
  cur_ = 0;

  Session::Init(new TransportMock);
  SetFlags(IGNORE_ACK);
}

SessionMock::~SessionMock() {
  for (size_t a = 0; a < actions_.size(); a++)
    delete actions_[a];

  delete static_cast<TransportMock*>(io_);
}

bool SessionMock::SendPacketOnly(gdb_rsp::Packet *packet) {
  if (PeekAction() == SEND) {
    Action *pact = GetAction();
    const char *str = packet->GetPayload();

    if (GetFlags() & DEBUG_SEND) port::IPlatform::LogInfo("TX %s\n", str);

    // Check if we match
    if (pact->str_ == str) return true;

    port::IPlatform::LogInfo("Mismatch:\n\tSENDING: %s\n\tEXPECT: %s\n",
                              str, pact->str_.data());
  }
  return false;
}

bool SessionMock::GetPacket(gdb_rsp::Packet *packet) {
  if (PeekAction() == RECV) {
    Action *pact = GetAction();
    packet->Clear();
    packet->AddString(pact->str_.data());
    if (GetFlags() & DEBUG_RECV) {
      port::IPlatform::LogInfo("RX %s\n", pact->str_.data());
    }
    return true;
  }
  return false;
}

bool SessionMock::DataAvailable() {
  return PeekAction() == RECV;
}

SessionMock::ActionType SessionMock::PeekAction() {
  if (cur_ < actions_.size()) {
    Action* pact = actions_[cur_];
    ActionType at = pact->type_;
    if (DISCONNECT == at) {
      connected_ = false;
    }
    return at;
  }
  return TIMEOUT;
}

SessionMock::Action* SessionMock::GetAction() {
  if (cur_ < actions_.size()) return actions_[cur_++];
  return NULL;
}

void SessionMock::AddAction(ActionType act, const char *str) {
  Action *pact = new Action;

  pact->type_ = act;
  pact->str_ = str;
  actions_.push_back(pact);
}

void SessionMock::AddTransaction(const char *snd, const char *rcv) {
  if (target_) {
    // If we are a target, we get a command, then respond
    AddAction(RECV, snd);
    AddAction(SEND, rcv);
  } else {
    // If we are a host, we send a command, and expect a reply
    AddAction(SEND, snd);
    AddAction(RECV, rcv);
  }
}

void AddMockInit(SessionMock* ses) {
  const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();

  // Setup supported query
  ses->AddTransaction(
    "qSupported",
    "PacketSize=7cf;qXfer:libraries:read+;qXfer:features:read+");

  // Setup arch query
  std::string str = "l<target><architecture>";
  str += abi->GetName();
  str += "</architecture></target>";
  ses->AddTransaction(
    "qXfer:features:read:target.xml:0,7cf",
    str.data());
}

void AddMockUpdate(SessionMock* ses, uint8_t sig) {
  const gdb_rsp::Abi* abi = gdb_rsp::Abi::Get();

  // Setup thread query
  ses->AddTransaction(
    "qfThreadInfo",
    "m1234");
  ses->AddTransaction(
    "qsThreadInfo",
    "l");
  ses->AddTransaction(
    "Hg1234",
    "OK");

  // Setup Register Query
  std::string str = "";
  port::IThread *thread = port::IThread::Acquire(0x1234, true);
  for (uint32_t a = 0; a < abi->GetRegisterCount(); a++) {
    char tmp[8];
    const Abi::RegDef* def = abi->GetRegisterDef(a);
    uint64_t val = static_cast<uint64_t>(a);
    uint8_t *pval = reinterpret_cast<uint8_t*>(&val);

    // Create a hex version of the register number of
    // the specified zero padded to the correct size
    for (uint32_t b = 0; b < def->bytes_; b++) {
      snprintf(tmp, sizeof(tmp), "%02x", pval[b]);
      str += tmp;
    }

    thread->SetRegister(a, &val, def->bytes_);
  }
  ses->AddTransaction("g", str.data());

  char tmp[8];
  snprintf(tmp, sizeof(tmp), "S%02x", sig);
  ses->AddTransaction("?", tmp);
}

// GetGoldenSessionMock
//
// The function bellow generates a mock session and preloads it
// with expected inputs and outputs for a common connection case.
// For GDB, that's:
//  1) qSupported - Querry for supported features
//  2) qXfer:features:read:target.xml:0,7cf - Query for target arch.
//  3) qfThreadInfo/qsThreadInfo - Query for active threads
//  4) Hg1234 - Set current register context to retreived thread id
//  5) g - Get current thread registers
//  6) disconnect
SessionMock *GetGoldenSessionMock(bool target, bool debug) {
  SessionMock   *ses = new SessionMock(target);

  if (debug) ses->SetFlags(Session::DEBUG_RECV | Session::DEBUG_SEND);

  AddMockInit(ses);
  AddMockUpdate(ses, 5);

  ses->AddAction(SessionMock::DISCONNECT, "");
  return ses;
}

}  // namespace gdb_rsp
