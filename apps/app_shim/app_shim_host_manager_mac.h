// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_channel_factory.h"

namespace base {
class FilePath;
}

namespace test {
class AppShimHostManagerTestApi;
}

// The AppShimHostManager receives connections from app shims on a UNIX
// socket (|factory_|) and creates a helper object to manage the connection.
class AppShimHostManager
    : public IPC::ChannelFactory::Delegate,
      public base::RefCountedThreadSafe<
          AppShimHostManager, content::BrowserThread::DeleteOnUIThread> {
 public:
  AppShimHostManager();

  // Init passes this AppShimHostManager to PostTask which requires it to have
  // a non-zero refcount. Therefore, Init cannot be called in the constructor
  // since the refcount is zero at that point.
  void Init();

  apps::ExtensionAppShimHandler* extension_app_shim_handler() {
    return &extension_app_shim_handler_;
  }

 private:
  friend class base::RefCountedThreadSafe<AppShimHostManager>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AppShimHostManager>;
  friend class test::AppShimHostManagerTestApi;
  virtual ~AppShimHostManager();

  // IPC::ChannelFactory::Delegate implementation.
  virtual void OnClientConnected(const IPC::ChannelHandle& handle) OVERRIDE;
  virtual void OnListenError() OVERRIDE;

  // The |factory_| must be created on a thread which allows blocking I/O, so
  // part of the initialization of this class must be carried out on the file
  // thread.
  void InitOnFileThread();

  // Called on the IO thread to begin listening for connections from app shims.
  void ListenOnIOThread();

  // If set, used instead of chrome::DIR_USER_DATA for placing the socket.
  static const base::FilePath* g_override_user_data_dir_;

  scoped_ptr<IPC::ChannelFactory> factory_;

  apps::ExtensionAppShimHandler extension_app_shim_handler_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManager);
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SHIM_HOST_MANAGER_MAC_H_
