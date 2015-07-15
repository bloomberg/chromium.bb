// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/mojo_persistent_cookie_store.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/services/network/network_service_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

namespace mojo {

MojoPersistentCookieStore::MojoPersistentCookieStore(
    NetworkServiceDelegate* network_service_delegate,
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    bool restore_old_session_cookies,
    net::CookieCryptoDelegate* crypto_delegate)
    : network_service_delegate_(network_service_delegate) {
  sql_cookie_store_ = new net::SQLitePersistentCookieStore(
      path,
      client_task_runner,
      background_task_runner,
      restore_old_session_cookies,
      crypto_delegate);
  network_service_delegate_->AddObserver(this);
}

void MojoPersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  if (sql_cookie_store_)
    sql_cookie_store_->Load(loaded_callback);
}

void MojoPersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& callback) {
  if (sql_cookie_store_)
    sql_cookie_store_->LoadCookiesForKey(key, callback);
}

void MojoPersistentCookieStore::AddCookie(const net::CanonicalCookie& cc) {
  if (sql_cookie_store_)
    sql_cookie_store_->AddCookie(cc);
}

void MojoPersistentCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  if (sql_cookie_store_)
    sql_cookie_store_->UpdateCookieAccessTime(cc);
}

void MojoPersistentCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  if (sql_cookie_store_)
    sql_cookie_store_->DeleteCookie(cc);
}

void MojoPersistentCookieStore::SetForceKeepSessionState() {
  if (sql_cookie_store_)
    sql_cookie_store_->SetForceKeepSessionState();
}

void MojoPersistentCookieStore::Flush(const base::Closure& callback) {
  if (sql_cookie_store_)
    sql_cookie_store_->Flush(callback);
}

MojoPersistentCookieStore::~MojoPersistentCookieStore() {
  RemoveObserver();
}

void MojoPersistentCookieStore::RemoveObserver() {
  if (network_service_delegate_) {
    network_service_delegate_->RemoveObserver(this);
    network_service_delegate_ = nullptr;
  }
}

namespace {

void SignalComplete(base::WaitableEvent* event) {
  event->Signal();
}

}  // namespace

void MojoPersistentCookieStore::OnIOWorkerThreadShutdown() {
  // We need to shut down synchronously here. This will block our thread until
  // the backend has shut down.
  base::WaitableEvent done_event(false, false);
  sql_cookie_store_->Close(base::Bind(&SignalComplete, &done_event));
  done_event.Wait();

  sql_cookie_store_ = NULL;

  RemoveObserver();
}

}  // namespace mojo
