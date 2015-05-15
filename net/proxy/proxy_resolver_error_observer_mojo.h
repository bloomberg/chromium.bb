// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_MOJO_H_
#define NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_MOJO_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "net/interfaces/proxy_resolver_service.mojom.h"
#include "net/proxy/proxy_resolver_error_observer.h"

namespace net {

// An implementation of ProxyResolverErrorObserver that forwards errors to an
// interfaces::ProxyResolverErrorObserver mojo interface.
class ProxyResolverErrorObserverMojo : public ProxyResolverErrorObserver {
 public:
  static scoped_ptr<ProxyResolverErrorObserver> Create(
      interfaces::ProxyResolverErrorObserverPtr error_observer);

  void OnPACScriptError(int line_number, const base::string16& error) override;

 private:
  explicit ProxyResolverErrorObserverMojo(
      interfaces::ProxyResolverErrorObserverPtr error_observer);
  ~ProxyResolverErrorObserverMojo() override;

  // |error_observer_| may only be accessed when running on |task_runner_|.
  interfaces::ProxyResolverErrorObserverPtr error_observer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Creating a new WeakPtr is only valid on the original thread, but copying an
  // existing WeakPtr is valid on any thread so keep |weak_this_| ready to copy.
  base::WeakPtr<ProxyResolverErrorObserverMojo> weak_this_;
  base::WeakPtrFactory<ProxyResolverErrorObserverMojo> weak_factory_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_MOJO_H_
