// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
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
  bool LaunchLoader(NaClProcessHost* client,
                    const std::wstring& loader_channel_id);

  // Called by NaClBrokerHost to notify the service that a loader was launched.
  void OnLoaderLaunched(const std::wstring& channel_id,
                        base::ProcessHandle handle);

  // Called by NaClProcessHost when a loader process is terminated
  void OnLoaderDied();

 private:
  typedef std::map<std::wstring, NaClProcessHost*>
      PendingLaunchesMap;

  friend struct DefaultSingletonTraits<NaClBrokerService>;

  NaClBrokerService();
  ~NaClBrokerService() {}

  NaClBrokerHost* GetBrokerHost();

  int loaders_running_;
  PendingLaunchesMap pending_launches_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerService);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROKER_SERVICE_WIN_H_
