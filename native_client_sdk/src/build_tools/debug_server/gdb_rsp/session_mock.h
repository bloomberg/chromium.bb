/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_GDB_RSP_SESSION_MOCK_H_
#define NATIVE_CLIENT_GDB_RSP_SESSION_MOCK_H_ 1

#include <string>
#include <vector>

#include "native_client/src/debug_server/gdb_rsp/packet.h"
#include "native_client/src/debug_server/gdb_rsp/session.h"
#include "native_client/src/debug_server/port/platform.h"
#include "native_client/src/debug_server/port/transport.h"

namespace gdb_rsp {

class SessionMock : public Session {
 public:
  enum ActionType {
    SEND,
    RECV,
    TIMEOUT,
    DISCONNECT
  };

  struct Action {
    std::string str_;
    ActionType  type_;
  };

  class TransportMock : public port::ITransport {
   public:
    virtual int32_t Read(void *ptr, int32_t len) {
      (void) ptr;
      (void) len;
      return -1;
    }
    virtual int32_t Write(const void *ptr, int32_t len) {
      (void) ptr;
      (void) len;
      return -1;
    }
    virtual bool ReadWaitWithTimeout(uint32_t ms) {
      (void) ms;
      return false;
    }
    virtual void Disconnect() { }
  };

  explicit SessionMock(bool target);
  ~SessionMock();

  bool SendPacketOnly(gdb_rsp::Packet *packet);
  bool GetPacket(gdb_rsp::Packet *packet);
  bool DataAvailable();
  ActionType PeekAction();
  Action *GetAction();

  void AddAction(ActionType act, const char *str);
  void AddTransaction(const char *snd, const char *rcv);

 private:
  std::vector<Action*> actions_;
  size_t cur_;
  bool target_;
};

SessionMock *GetGoldenSessionMock(bool target, bool debug);
void AddMockInit(SessionMock *ses);
void AddMockUpdate(SessionMock *ses, uint8_t sig);



}  // namespace gdb_rsp

#endif  // NATIVE_CLIENT_GDB_RSP_SESSION_MOCK_H_
