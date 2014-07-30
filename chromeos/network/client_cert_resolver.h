// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/network_policy_observer.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace base {
class TaskRunner;
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

  ClientCertResolver();
  virtual ~ClientCertResolver();

  void Init(NetworkStateHandler* network_state_handler,
            ManagedNetworkConfigurationHandler* managed_network_config_handler);

  // Sets the task runner that any slow calls will be made from, e.g. calls
  // to the NSS database. If not set, uses base::WorkerPool.
  void SetSlowTaskRunnerForTest(
      const scoped_refptr<base::TaskRunner>& task_runner);

  // Returns true and sets the Shill properties that have to be configured in
  // |shill_properties| if the certificate pattern |pattern| could be resolved.
  // Returns false otherwise and sets empty Shill properties to clear the
  // certificate configuration.
  static bool ResolveCertificatePatternSync(
      const client_cert::ConfigType client_cert_type,
      const CertificatePattern& pattern,
      base::DictionaryValue* shill_properties);

 private:
   // NetworkStateHandlerObserver overrides
  virtual void NetworkListChanged() OVERRIDE;

  // CertLoader::Observer overrides
  virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                    bool initial_load) OVERRIDE;

  // NetworkPolicyObserver overrides
  virtual void PolicyApplied(const std::string& service_path) OVERRIDE;

  // Check which networks of |networks| are configured with a client certificate
  // pattern. Search for certificates, on the worker thread, and configure the
  // networks for which a matching cert is found (see ConfigureCertificates).
  void ResolveNetworks(const NetworkStateHandler::NetworkStateList& networks);

  // |matches| contains networks for which a matching certificate was found.
  // Configures these networks.
  void ConfigureCertificates(std::vector<NetworkAndMatchingCert>* matches);

  // The set of networks that were checked/resolved in previous passes. These
  // networks are skipped in the NetworkListChanged notification.
  std::set<std::string> resolved_networks_;

  // Unowned associated (global or test) instance.
  NetworkStateHandler* network_state_handler_;

  // Unowned associated (global or test) instance.
  ManagedNetworkConfigurationHandler* managed_network_config_handler_;

  // TaskRunner for slow tasks.
  scoped_refptr<base::TaskRunner> slow_task_runner_for_test_;

  base::WeakPtrFactory<ClientCertResolver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientCertResolver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_RESOLVER_H_
