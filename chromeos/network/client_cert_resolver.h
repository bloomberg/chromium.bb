// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace base {
class Clock;
}

namespace chromeos {

class NetworkState;
class ManagedNetworkConfigurationHandler;

// Observes the known networks. If a network is configured with a client
// certificate pattern, this class searches for a matching client certificate.
// Each time it finds a match, it configures the network accordingly.
class CHROMEOS_EXPORT ClientCertResolver : public NetworkStateHandlerObserver,
                                           public CertLoader::Observer,
                                           public NetworkPolicyObserver {
 public:
  struct NetworkAndMatchingCert;

  class Observer {
   public:
    // Called every time resolving of client certificate patterns finishes,
    // no resolve requests are pending and no tasks are running.
    // |network_properties_changed| will be true if any network properties were
    // changed by this resolver since the last notification.
    virtual void ResolveRequestCompleted(bool network_properties_changed) = 0;

   protected:
    virtual ~Observer() {}

   private:
    DISALLOW_ASSIGN(Observer);
  };

  ClientCertResolver();
  ~ClientCertResolver() override;

  void Init(NetworkStateHandler* network_state_handler,
            ManagedNetworkConfigurationHandler* managed_network_config_handler);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if any resolve tasks are running. Every time a task finishes
  // and no further requests are pending, a notification is sent, see
  // |Observer|.
  bool IsAnyResolveTaskRunning() const;

  // Sets the clock for testing. This clock is used when checking the
  // certificates for expiration.
  void SetClockForTesting(base::Clock* clock);

  // Returns true and sets the Shill properties that have to be configured in
  // |shill_properties| if the certificate pattern |pattern| could be resolved.
  // Returns false otherwise and sets empty Shill properties to clear the
  // certificate configuration.
  // Note that it uses the global clock when checking the certificates for
  // expiration.
  static bool ResolveCertificatePatternSync(
      const client_cert::ConfigType client_cert_type,
      const client_cert::ClientCertConfig& client_cert_config,
      base::DictionaryValue* shill_properties);

 private:
  // NetworkStateHandlerObserver overrides
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // CertLoader::Observer overrides
  void OnCertificatesLoaded(
      const net::ScopedCERTCertificateList& cert_list) override;

  // NetworkPolicyObserver overrides
  void PolicyAppliedToNetwork(const std::string& service_path) override;

  // Check which networks of |networks| are configured with a client certificate
  // pattern. Search for certificates, on the worker thread, and configure the
  // networks for which a matching cert is found (see ConfigureCertificates).
  void ResolveNetworks(const NetworkStateHandler::NetworkStateList& networks);

  // Resolves certificates for the pending networks. This will always trigger a
  // ResolveRequestCompleted notification, even if the queue is empty.
  void ResolvePendingNetworks();

  // |matches| contains networks for which a matching certificate was found.
  // Configures these networks.
  void ConfigureCertificates(
      std::unique_ptr<std::vector<NetworkAndMatchingCert>> matches);

  // Trigger a ResolveRequestCompleted event on all observers.
  void NotifyResolveRequestCompleted();

  // Returns Time::Now() unless a mock clock has been installed with
  // SetClockForTesting, in which case the time according to that clock is used
  // instead.
  base::Time Now() const;

  base::ObserverList<Observer, true> observers_;

  // The set of networks that were checked/resolved in previous passes. These
  // networks are skipped in the NetworkListChanged notification.
  std::set<std::string> resolved_networks_;

  // The list of network paths that still have to be resolved.
  std::set<std::string> queued_networks_to_resolve_;

  // True if currently a resolve task is running.
  bool resolve_task_running_;

  // True if any network properties were changed since the last notification to
  // observers.
  bool network_properties_changed_;

  // Unowned associated (global or test) instance.
  NetworkStateHandler* network_state_handler_;

  // Unowned associated (global or test) instance.
  ManagedNetworkConfigurationHandler* managed_network_config_handler_;

  // Can be set for testing.
  base::Clock* testing_clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientCertResolver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertResolver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_
