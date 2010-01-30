// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_THREAD_H_
#define CHROME_NACL_NACL_THREAD_H_

#include "base/file_path.h"
#include "base/native_library.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/nacl_types.h"

// The NaClThread class represents a background thread where NaCl app gets
// started.
class NaClThread : public ChildThread {
 public:
  NaClThread();
  ~NaClThread();
  // Returns the one NaCl thread.
  static NaClThread* current();

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);
  void OnStartSelLdr(int channel_descriptor, nacl::FileDescriptor handle);

  // TODO(gregoryd): do we need to override Cleanup as in PluginThread?
  DISALLOW_COPY_AND_ASSIGN(NaClThread);
};

#endif  // CHROME_NACL_NACL_THREAD_H_
