// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_
#define APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "apps/app_shim/unix_domain_socket_acceptor.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class FilePath;
}

namespace test {
class AppShimHostManagerTestApi;
}

// The AppShimHostManager receives connections from app shims on a UNIX
// socket (|acceptor_|) and creates a helper object to manage the connection.
class AppShimHostManager : public apps::UnixDomainSocketAcceptor::Delegate,
                           public base::RefCountedThreadSafe<
                               AppShimHostManager,
                               content::BrowserThread::DeleteOnUIThread> {
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

  // UnixDomainSocketAcceptor::Delegate implementation.
  virtual void OnClientConnected(const IPC::ChannelHandle& handle) OVERRIDE;
  virtual void OnListenError() OVERRIDE;

  // The |acceptor_| must be created on a thread which allows blocking I/O, so
  // part of the initialization of this class must be carried out on the file
  // thread.
  void InitOnFileThread();

  // Called on the IO thread to begin listening for connections from app shims.
  void ListenOnIOThread();

  // The AppShimHostManager is only initialized if the Chrome process
  // successfully took the singleton lock. This prevents subsequent processes
  // from deleting existing app shim socket files.
  bool did_init_;

  base::FilePath directory_in_tmp_;

  scoped_ptr<apps::UnixDomainSocketAcceptor> acceptor_;

  apps::ExtensionAppShimHandler extension_app_shim_handler_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManager);
};

#endif  // APPS_APP_SHIM_APP_SHIM_HOST_MANAGER_MAC_H_
