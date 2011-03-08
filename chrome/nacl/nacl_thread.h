// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_THREAD_H_
#define CHROME_NACL_NACL_THREAD_H_
#pragma once

#include "base/native_library.h"
#include "chrome/common/nacl_types.h"
#include "content/common/child_thread.h"

// The NaClThread class represents a background thread where NaCl app gets
// started.
class NaClThread : public ChildThread {
 public:
  explicit NaClThread(bool debug);
  ~NaClThread();
  // Returns the one NaCl thread.
  static NaClThread* current();

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  void OnStartSelLdr(std::vector<nacl::FileDescriptor> handles);

  int debug_enabled_;

  // TODO(gregoryd): do we need to override Cleanup as in PluginThread?
  DISALLOW_COPY_AND_ASSIGN(NaClThread);
};

#endif  // CHROME_NACL_NACL_THREAD_H_
