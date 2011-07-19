// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_BROKER_THREAD_H_
#define CHROME_NACL_BROKER_THREAD_H_
#pragma once

#include "base/process.h"
#include "chrome/common/nacl_types.h"
#include "content/common/child_thread.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

// The BrokerThread class represents the thread that handles the messages from
// the browser process and starts NaCl loader processes.
class NaClBrokerThread : public ChildThread {
 public:
  NaClBrokerThread();
  ~NaClBrokerThread();
  // Returns the one NaCl thread.
  static NaClBrokerThread* current();

  virtual void OnChannelConnected(int32 peer_pid);

 private:
  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  void OnLaunchLoaderThroughBroker(const std::wstring& loader_channel_id);
  void OnShareBrowserHandle(int browser_handle);
  void OnStopBroker();

  base::ProcessHandle browser_handle_;
  sandbox::BrokerServices* broker_services_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerThread);
};

#endif  // CHROME_NACL_BROKER_THREAD_H_
