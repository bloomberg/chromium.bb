// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_thread.h"

#include "build/build_config.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/nacl_messages.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"

int SelMain(const int desc, const NaClHandle handle);

NaClThread::NaClThread() {
}

NaClThread::~NaClThread() {
}

NaClThread* NaClThread::current() {
  return static_cast<NaClThread*>(ChildThread::current());
}

void NaClThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(NaClThread, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_StartSelLdr, OnStartSelLdr)
  IPC_END_MESSAGE_MAP()
}

void NaClThread::OnStartSelLdr(const int channel_descriptor,
                               const nacl::FileDescriptor handle) {
  SelMain(channel_descriptor, NATIVE_HANDLE(handle));
}
