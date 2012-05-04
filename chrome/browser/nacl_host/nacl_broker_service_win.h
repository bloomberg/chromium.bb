// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nacl_host/nacl_broker_host_win.h"

class NaClProcessHost;

class NaClBrokerService {
 public:
  // Returns the NaClBrokerService singleton.
  static NaClBrokerService* GetInstance();

  // Can be called several times, must be called before LaunchLoader.
  bool StartBroker();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(base::WeakPtr<NaClProcessHost> client,
                    const std::string& loader_channel_id);

  // Called by NaClBrokerHost to notify the service that a loader was launched.
  void OnLoaderLaunched(const std::string& channel_id,
                        base::ProcessHandle handle);

  // Called by NaClProcessHost when a loader process is terminated
  void OnLoaderDied();

  bool LaunchDebugExceptionHandler(base::WeakPtr<NaClProcessHost> client,
                                   int32 pid,
                                   base::ProcessHandle process_handle,
                                   const std::string& startup_info);

  // Called by NaClBrokerHost to notify the service that a debug
  // exception handler was started.
  void OnDebugExceptionHandlerLaunched(int32 pid, bool success);

 private:
  typedef std::map<std::string, base::WeakPtr<NaClProcessHost> >
      PendingLaunchesMap;
  typedef std::map<int, base::WeakPtr<NaClProcessHost> >
      PendingDebugExceptionHandlersMap;

  friend struct DefaultSingletonTraits<NaClBrokerService>;

  NaClBrokerService();
  ~NaClBrokerService() {}

  NaClBrokerHost* GetBrokerHost();

  int loaders_running_;
  PendingLaunchesMap pending_launches_;
  PendingDebugExceptionHandlersMap pending_debuggers_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerService);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
