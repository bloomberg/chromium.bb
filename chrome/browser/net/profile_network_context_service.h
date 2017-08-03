// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "content/public/common/network_service.mojom.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// KeyedService that initializes and provides access to the NetworkContexts for
// a Profile. This will eventually replace ProfileIOData.
class ProfileNetworkContextService : public KeyedService {
 public:
  explicit ProfileNetworkContextService(Profile* profile);
  ~ProfileNetworkContextService() override;

  // Initializes |*network_context_params| to set up the ProfileIOData's
  // main URLRequestContext and |*network_context_request| to be one end of a
  // Mojo pipe to be bound to the NetworkContext for that URLRequestContext.
  // The caller will need to send these parameters to the IOThread's in-process
  // NetworkService. This class retains the NetworkContext at the other end of
  // the |*network_context_request| pipe for later vending.
  //
  // If the network service is disabled, MainContext() will return that end of
  // the pipe.  In this case, all requests associated with this profile will use
  // the associated URLRequestContext (either through MainContext() or
  // directly).
  //
  // If the network service is enabled, MainContext() will instead return a
  // network context vended by the network service's NetworkService (Instead of
  // the IOThread's in-process one).  In this case, the ProfileIOData's
  // URLRequest context will be configured not to use on-disk storage (so as not
  // to conflict with the network service vended context), and will only be used
  // for legacy requests that use it directly.
  //
  // Must be called before anything uses the NetworkContext vended by this
  // class.
  void SetUpProfileIODataMainContext(
      content::mojom::NetworkContextRequest* network_context_request,
      content::mojom::NetworkContextParamsPtr* network_context_params);

  // Returns the main NetworkContext for the BrowserContext. If the network
  // service is disabled, this will be the
  // ProfileIOData NetworkContext set up above.  Otherwise, it will be a
  // NetworkContext vended from the network service.
  content::mojom::NetworkContext* MainContext();

  // Creates the main NetworkContext for the BrowserContext, using the network
  // service. May only be called when the network service is enabled. Must only
  // be called once for a profile, from the ChromeContentBrowserClient.
  content::mojom::NetworkContextPtr CreateMainNetworkContext();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Checks |quic_allowed_|, and disables QUIC if needed.
  void DisableQuicIfNotAllowed();

  Profile* const profile_;

  // This is a NetworkContext that wraps ProfileIOData's main URLRequestContext.
  // Always initialized in SetUpProfileIODataMainContext, but it's only returned
  // by Context() when the network service is disabled.
  content::mojom::NetworkContextPtr profile_io_data_main_network_context_;

  BooleanPrefMember quic_allowed_;

  DISALLOW_COPY_AND_ASSIGN(ProfileNetworkContextService);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
