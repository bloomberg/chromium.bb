// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_
#define CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/cert_loader.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

// Migrates network configurations from deprecated CaCertNSS properties to
// CaCertPEM.
class CHROMEOS_EXPORT NetworkCertMigrator : public NetworkStateHandlerObserver,
                                            public CertLoader::Observer {
 public:
  virtual ~NetworkCertMigrator();

 private:
  friend class NetworkHandler;
  friend class NetworkCertMigratorTest;
  class MigrationTask;

  NetworkCertMigrator();
  void Init(NetworkStateHandler* network_state_handler);

   // NetworkStateHandlerObserver overrides
  virtual void NetworkListChanged() OVERRIDE;

  // CertLoader::Observer overrides
  virtual void OnCertificatesLoaded(const net::CertificateList& cert_list,
                                    bool initial_load) OVERRIDE;

  // Unowned associated NetworkStateHandler* (global or test instance).
  NetworkStateHandler* network_state_handler_;

  base::WeakPtrFactory<NetworkCertMigrator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertMigrator);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CERT_MIGRATOR_H_
