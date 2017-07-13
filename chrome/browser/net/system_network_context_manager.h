// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_

#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/public/common/network_service.mojom.h"

// Global object that lives on the UI thread. Responsible for creating and
// managing access to the system NetworkContext. This NetworkContext is intended
// for requests not associated with a profile. It stores no data on disk, and
// has no HTTP cache, but it does have ephemeral cookie and channel ID stores.
// It also does not have access to HTTP proxy auth information the user has
// entered or that comes from extensions, and similarly, has no
// extension-provided per-profile proxy configuration information.
//
// The "system" NetworkContext will either share a URLRequestContext with
// IOThread's SystemURLRequestContext and be part of IOThread's NetworkService
// (If the network service is disabled) or be an independent NetworkContext
// using the actual network service.
//
// This class is intended to eventually replace IOThread. Handling the two cases
// differently allows this to be used in production without breaking anything or
// requiring two separate paths, while IOThread consumers slowly transition over
// to being compatible with the network service.
class SystemNetworkContextManager {
 public:
  // Initializes |network_context_params| as needed to set up a system
  // NetworkContext. If the network service is disabled,
  // |network_context_request| will be for the NetworkContext used by the
  // SystemNetworkContextManager. Otherwise, this method can still be used to
  // help set up the IOThread's in-process URLRequestContext, and
  // |network_context_request| will still be populated, but the associated
  // NetworkContext will not be used by the SystemNetworkContextManager.
  //
  // Must be called before the system NetworkContext is first used.
  static void SetUp(
      content::mojom::NetworkContextRequest* network_context_request,
      content::mojom::NetworkContextParamsPtr* network_context_params);

  // Returns the System NetworkContext. May only be called after SetUp().
  static content::mojom::NetworkContext* Context();

 private:
  static SystemNetworkContextManager* GetInstance();

  friend struct base::LazyInstanceTraitsBase<SystemNetworkContextManager>;

  SystemNetworkContextManager();
  ~SystemNetworkContextManager();

  // Gets the system NetworkContext, creating it if needed.
  content::mojom::NetworkContext* GetContext();

  // NetworkContext using the network service, if the/ network service is
  // enabled. nullptr, otherwise.
  content::mojom::NetworkContextPtr network_service_network_context_;

  // This is a NetworkContext that wraps the IOThread's SystemURLRequestContext.
  // Always initialized in SetUp, but it's only returned by Context() when the
  // network service is disabled.
  content::mojom::NetworkContextPtr io_thread_network_context_;

  DISALLOW_COPY_AND_ASSIGN(SystemNetworkContextManager);
};

#endif  // CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
