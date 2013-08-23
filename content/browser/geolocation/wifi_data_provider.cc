// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/wifi_data_provider.h"

namespace content {

// static
WifiDataProvider* WifiDataProvider::instance_ = NULL;

// static
WifiDataProvider::ImplFactoryFunction WifiDataProvider::factory_function_ =
    DefaultFactoryFunction;

// static
WifiDataProvider* WifiDataProvider::Register(WifiDataUpdateCallback* callback) {
  bool need_to_start_data_provider = false;
  if (!instance_) {
    instance_ = new WifiDataProvider();
    need_to_start_data_provider = true;
  }
  DCHECK(instance_);
  instance_->AddCallback(callback);
  // Start the provider after adding the callback, to avoid any race in
  // it running early.
  if (need_to_start_data_provider)
    instance_->StartDataProvider();
  return instance_;
}

WifiDataProviderImplBase::WifiDataProviderImplBase()
    : container_(NULL),
      client_loop_(base::MessageLoop::current()) {
  DCHECK(client_loop_);
}

WifiDataProviderImplBase::~WifiDataProviderImplBase() {
}

void WifiDataProviderImplBase::SetContainer(WifiDataProvider* container) {
  container_ = container;
}

void WifiDataProviderImplBase::AddCallback(WifiDataUpdateCallback* callback) {
  callbacks_.insert(callback);
}

bool WifiDataProviderImplBase::RemoveCallback(
    WifiDataUpdateCallback* callback) {
  return callbacks_.erase(callback) == 1;
}

bool WifiDataProviderImplBase::has_callbacks() const {
  return !callbacks_.empty();
}

void WifiDataProviderImplBase::RunCallbacks() {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &WifiDataProviderImplBase::DoRunCallbacks,
      this));
}

bool WifiDataProviderImplBase::CalledOnClientThread() const {
  return base::MessageLoop::current() == this->client_loop_;
}

base::MessageLoop* WifiDataProviderImplBase::client_loop() const {
  return client_loop_;
}

void WifiDataProviderImplBase::DoRunCallbacks() {
  // It's possible that all the callbacks (and the container) went away
  // whilst this task was pending. This is fine; the loop will be a no-op.
  CallbackSet::const_iterator iter = callbacks_.begin();
  while (iter != callbacks_.end()) {
    WifiDataUpdateCallback* callback = *iter;
    ++iter;  // Advance iter before running, in case callback unregisters.
    callback->Run(container_);
  }
}

WifiDataProvider::WifiDataProvider() {
  DCHECK(factory_function_);
  impl_ = (*factory_function_)();
  DCHECK(impl_.get());
  impl_->SetContainer(this);
}

WifiDataProvider::~WifiDataProvider() {
  DCHECK(impl_.get());
  impl_->SetContainer(NULL);
}

}  // namespace content
