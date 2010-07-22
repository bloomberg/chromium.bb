// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/nacl/nacl_thread.h"

#include "chrome/common/notification_service.h"
#include "chrome/common/nacl_messages.h"

// This is ugly.  We need an interface header file for the exported
// sel_ldr interfaces.
// TODO(gregoryd,sehr): Add an interface header.
#if defined(OS_WIN)
typedef HANDLE NaClHandle;
#else
typedef int NaClHandle;
#endif  // NaClHandle

// This is currently necessary because we have a conflict between
// NaCl's "struct NaClThread" and Chromium's "class NaClThread".
extern "C" int NaClMainForChromium(int handle_count, const NaClHandle* handles);

NaClThread::NaClThread() {
}

NaClThread::~NaClThread() {
}

NaClThread* NaClThread::current() {
  return static_cast<NaClThread*>(ChildThread::current());
}

void NaClThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(NaClThread, msg)
    IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStartSelLdr)
  IPC_END_MESSAGE_MAP()
}

void NaClThread::OnStartSelLdr(int channel_descriptor,
                               nacl::FileDescriptor handle) {
  NaClHandle nacl_handle = nacl::ToNativeHandle(handle);
  NaClMainForChromium(/* handle_count= */ 1, &nacl_handle);
}
