// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/async_method_caller.h"

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

using chromeos::DBusThreadManager;

namespace cryptohome {

namespace {

AsyncMethodCaller* g_async_method_caller = NULL;

// The implementation of AsyncMethodCaller
class AsyncMethodCallerImpl : public AsyncMethodCaller {
 public:
  AsyncMethodCallerImpl() : weak_ptr_factory_(this) {
    DBusThreadManager::Get()->GetCryptohomeClient()->SetAsyncCallStatusHandler(
        base::Bind(&AsyncMethodCallerImpl::HandleAsyncResponse,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~AsyncMethodCallerImpl() {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        ResetAsyncCallStatusHandler();
  }

  virtual void AsyncCheckKey(const std::string& user_email,
                             const std::string& passhash,
                             Callback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncCheckKey(user_email, passhash, base::Bind(
            &AsyncMethodCallerImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async check of user's key."));
  }

  virtual void AsyncMigrateKey(const std::string& user_email,
                               const std::string& old_hash,
                               const std::string& new_hash,
                               Callback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMigrateKey(user_email, old_hash, new_hash, base::Bind(
            &AsyncMethodCallerImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate aync migration of user's key"));
  }

  virtual void AsyncMount(const std::string& user_email,
                          const std::string& passhash,
                          int flags,
                          Callback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMount(user_email, passhash, flags, base::Bind(
            &AsyncMethodCallerImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async mount of cryptohome."));
  }

  virtual void AsyncMountGuest(Callback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncMountGuest(base::Bind(
            &AsyncMethodCallerImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async mount of cryptohome."));
  }

  virtual void AsyncRemove(const std::string& user_email,
                           Callback callback) OVERRIDE {
    DBusThreadManager::Get()->GetCryptohomeClient()->
        AsyncRemove(user_email, base::Bind(
            &AsyncMethodCallerImpl::RegisterAsyncCallback,
            weak_ptr_factory_.GetWeakPtr(),
            callback,
            "Couldn't initiate async removal of cryptohome."));
  }

 private:
  struct CallbackElement {
    CallbackElement() {}
    explicit CallbackElement(
        const AsyncMethodCaller::Callback& callback)
        : callback(callback),
          proxy(base::MessageLoopProxy::current()) {
    }
    AsyncMethodCaller::Callback callback;
    scoped_refptr<base::MessageLoopProxy> proxy;
  };

  typedef base::hash_map<int, CallbackElement> CallbackMap;

  // Hanldes the response for async calls.
  // Below is described how async calls work.
  // 1. CryptohomeClient::AsyncXXX returns "async ID".
  // 2. RegisterAsyncCallback registers the "async ID" with the user-provided
  //    callback.
  // 3. Cryptohome will return the result asynchronously as a signal with
  //    "async ID"
  // 4. "HandleAsyncResponse" handles the result signal and call the registered
  //    callback associated with the "async ID".
  void HandleAsyncResponse(int async_id, bool return_status, int return_code) {
    const CallbackMap::iterator it = callback_map_.find(async_id);
    if (it == callback_map_.end()) {
      LOG(ERROR) << "Received signal for unknown async_id " << async_id;
      return;
    }
    it->second.proxy->PostTask(FROM_HERE,
        base::Bind(it->second.callback,
                   return_status,
                   static_cast<MountError>(return_code)));
    callback_map_.erase(it);
  }

  // Registers a callback which is called when the result for AsyncXXX is ready.
  void RegisterAsyncCallback(
      Callback callback, const char* error, int async_id) {
    if (async_id == 0) {
      LOG(ERROR) << error;
      return;
    }
    VLOG(1) << "Adding handler for " << async_id;
    DCHECK_EQ(callback_map_.count(async_id), 0U);
    callback_map_[async_id] = CallbackElement(callback);
  }

  base::WeakPtrFactory<AsyncMethodCallerImpl> weak_ptr_factory_;
  CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(AsyncMethodCallerImpl);
};

}  // namespace

// static
void AsyncMethodCaller::Initialize() {
  if (g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller was already initialized";
    return;
  }
  g_async_method_caller = new AsyncMethodCallerImpl();
  VLOG(1) << "AsyncMethodCaller initialized";
}

// static
void AsyncMethodCaller::InitializeForTesting(
    AsyncMethodCaller* async_method_caller) {
  if (g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller was already initialized";
    return;
  }
  g_async_method_caller = async_method_caller;
  VLOG(1) << "AsyncMethodCaller initialized";
}

// static
void AsyncMethodCaller::Shutdown() {
  if (!g_async_method_caller) {
    LOG(WARNING) << "AsyncMethodCaller::Shutdown() called with NULL manager";
    return;
  }
  delete g_async_method_caller;
  g_async_method_caller = NULL;
  VLOG(1) << "AsyncMethodCaller Shutdown completed";
}

// static
AsyncMethodCaller* AsyncMethodCaller::GetInstance() {
  return g_async_method_caller;
}

}  // namespace cryptohome
