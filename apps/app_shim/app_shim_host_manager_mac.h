// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel_factory.h"

// The AppShimHostManager receives connections from app shims on a UNIX
// socket (|factory_|) and creates a helper object to manage the connection.
class AppShimHostManager
    : public IPC::ChannelFactory::Delegate,
      public base::SupportsWeakPtr<AppShimHostManager> {
 public:
  AppShimHostManager();
  virtual ~AppShimHostManager();

 private:
  // IPC::ChannelFactory::Delegate implementation.
  virtual void OnClientConnected(const IPC::ChannelHandle& handle) OVERRIDE;
  virtual void OnListenError() OVERRIDE;

  // The |factory_| must be created on a thread which allows blocking I/O, so
  // part of the initialization of this class must be carried out on the file
  // thread.
  void InitOnFileThread();

  // Called on the IO thread to begin listening for connections from app shims.
  void ListenOnIOThread();

  scoped_ptr<IPC::ChannelFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManager);
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_
