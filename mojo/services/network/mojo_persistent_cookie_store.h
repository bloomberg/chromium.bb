// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_MOJO_PERSISTENT_COOKIE_STORE_H_
#define MOJO_SERVICES_NETWORK_MOJO_PERSISTENT_COOKIE_STORE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "mojo/services/network/network_service_delegate_observer.h"
#include "net/cookies/cookie_monster.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace net {
class SQLitePersistentCookieStore;
}  // namespace net

namespace mojo {
class NetworkServiceDelegate;

// A PersistentCookieStore that listens to NetworkContext's and tries to
// gracefully shutdown when our ApplicationConnection is about to be closed.
class MojoPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore,
      public NetworkServiceDelegateObserver {
 public:
  MojoPersistentCookieStore(
      NetworkServiceDelegate* network_service_delegate,
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      bool restore_old_session_cookies,
      net::CookieCryptoDelegate* crypto_delegate);

  // CookieMonster::PersistentCookieStore:
  void Load(const LoadedCallback& loaded_callback) override;
  void LoadCookiesForKey(const std::string& key,
                         const LoadedCallback& callback) override;
  void AddCookie(const net::CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override;
  void DeleteCookie(const net::CanonicalCookie& cc) override;
  void SetForceKeepSessionState() override;
  void Flush(const base::Closure& callback) override;

  // NetworkServiceDelegateObserver:
  void OnIOWorkerThreadShutdown() override;

 private:
  ~MojoPersistentCookieStore() override;

  void RemoveObserver();

  NetworkServiceDelegate* network_service_delegate_;

  // We own the |sql_cookie_store_| that we proxy for. We delete this during
  // OnIOWorkerThreadShutdown().
  scoped_refptr<net::SQLitePersistentCookieStore> sql_cookie_store_;

  DISALLOW_COPY_AND_ASSIGN(MojoPersistentCookieStore);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_MOJO_PERSISTENT_COOKIE_STORE_H_
