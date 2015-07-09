// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_error_observer_mojo.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/common/common_type_converters.h"

namespace net {

// static
scoped_ptr<ProxyResolverErrorObserver> ProxyResolverErrorObserverMojo::Create(
    interfaces::ProxyResolverErrorObserverPtr error_observer) {
  if (!error_observer)
    return nullptr;

  return scoped_ptr<ProxyResolverErrorObserver>(
      new ProxyResolverErrorObserverMojo(error_observer.Pass()));
}

void ProxyResolverErrorObserverMojo::OnPACScriptError(
    int line_number,
    const base::string16& error) {
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&ProxyResolverErrorObserverMojo::OnPACScriptError,
                              weak_this_, line_number, error));
    return;
  }
  error_observer_->OnPacScriptError(line_number, mojo::String::From(error));
}

ProxyResolverErrorObserverMojo::ProxyResolverErrorObserverMojo(
    interfaces::ProxyResolverErrorObserverPtr error_observer)
    : error_observer_(error_observer.Pass()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

ProxyResolverErrorObserverMojo::~ProxyResolverErrorObserverMojo() = default;

}  // namespace net
