// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_
#define NET_PROXY_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "mojo/common/common_type_converters.h"
#include "net/dns/host_resolver_mojo.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver_v8_tracing.h"

namespace net {

// An implementation of ProxyResolverV8Tracing::Bindings that forwards requests
// onto a Client mojo interface. Alert() and OnError() may be called from any
// thread; when they are called from another thread, the calls are proxied to
// the origin task runner. GetHostResolver() and GetBoundNetLog() may only be
// called from the origin task runner.
template <typename Client>
class MojoProxyResolverV8TracingBindings
    : public ProxyResolverV8Tracing::Bindings,
      public HostResolverMojo::Impl {
 public:
  explicit MojoProxyResolverV8TracingBindings(base::WeakPtr<Client> client)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        client_(client),
        host_resolver_(this) {}

  // ProxyResolverV8Tracing::Bindings overrides.
  void Alert(const base::string16& message) override {
    if (!task_runner_->BelongsToCurrentThread()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MojoProxyResolverV8TracingBindings::AlertOnTaskRunner,
                     client_, message));
      return;
    }
    AlertOnTaskRunner(client_, message);
  }

  void OnError(int line_number, const base::string16& message) override {
    if (!task_runner_->BelongsToCurrentThread()) {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&MojoProxyResolverV8TracingBindings::OnErrorOnTaskRunner,
                     client_, line_number, message));
      return;
    }
    OnErrorOnTaskRunner(client_, line_number, message);
  }

  HostResolver* GetHostResolver() override {
    DCHECK(task_runner_->BelongsToCurrentThread());
    return &host_resolver_;
  }

  BoundNetLog GetBoundNetLog() override { return BoundNetLog(); }

 private:
  static void AlertOnTaskRunner(base::WeakPtr<Client> client,
                                const base::string16& message) {
    if (client)
      client->Alert(mojo::String::From(message));
  }

  static void OnErrorOnTaskRunner(base::WeakPtr<Client> client,
                                  int line_number,
                                  const base::string16& message) {
    if (client)
      client->OnError(line_number, mojo::String::From(message));
  }

  // HostResolverMojo::Impl override.
  void ResolveDns(interfaces::HostResolverRequestInfoPtr request_info,
                  interfaces::HostResolverRequestClientPtr client) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    DCHECK(client_);
    client_->ResolveDns(request_info.Pass(), client.Pass());
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<Client> client_;
  HostResolverMojo host_resolver_;
};

}  // namespace net

#endif  // NET_PROXY_MOJO_PROXY_RESOLVER_V8_TRACING_BINDINGS_H_
