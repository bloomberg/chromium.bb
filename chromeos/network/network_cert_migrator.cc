// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

enum UMANetworkType {
  UMA_NETWORK_TYPE_EAP,
  UMA_NETWORK_TYPE_OPENVPN,
  UMA_NETWORK_TYPE_IPSEC,
  UMA_NETWORK_TYPE_SIZE,
};

// Copied from x509_certificate_model_nss.cc
std::string GetNickname(const net::X509Certificate& cert) {
  if (!cert.os_cert_handle()->nickname)
    return std::string();
  std::string name = cert.os_cert_handle()->nickname;
  // Hack copied from mozilla: Cut off text before first :, which seems to
  // just be the token name.
  size_t colon_pos = name.find(':');
  if (colon_pos != std::string::npos)
    name = name.substr(colon_pos + 1);
  return name;
}

}  // namespace

// Migrates each network of |networks| with a deprecated CaCertNss property to
// the respective CaCertPEM property and fixes an invalid or missing slot ID of
// a client certificate configuration.
//
// If a network already has a CaCertPEM property, then the NssProperty is
// cleared. Otherwise, the NssProperty is compared with
// the nickname of each certificate of |certs|. If a match is found, the
// CaCertPemProperty is set and the NssProperty is cleared.
//
// If a network with a client certificate configuration (i.e. a PKCS11 ID) is
// found, the configured client certificate is looked up.
// If the certificate is found, the currently configured slot ID (if any) is
// compared with the actual slot ID of the certificate and if required updated.
// If the certificate is not found, the client certificate configuration is
// removed.
//
// Only if necessary, a network will be notified.
class NetworkCertMigrator::MigrationTask
    : public base::RefCounted<MigrationTask> {
 public:
  MigrationTask(const net::CertificateList& certs,
                const base::WeakPtr<NetworkCertMigrator>& cert_migrator)
      : certs_(certs),
        cert_migrator_(cert_migrator) {
  }

  void Run(const NetworkStateHandler::NetworkStateList& networks) {
    // Request properties for each network that has a CaCertNssProperty set
    // or which could be configured with a client certificate.
    for (NetworkStateHandler::NetworkStateList::const_iterator it =
             networks.begin(); it != networks.end(); ++it) {
      if (!(*it)->HasCACertNSS() &&
          (*it)->security() != shill::kSecurity8021x &&
          (*it)->type() != shill::kTypeVPN &&
          (*it)->type() != shill::kTypeEthernetEap) {
        continue;
      }
      const std::string& service_path = (*it)->path();
      DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
          dbus::ObjectPath(service_path),
          base::Bind(&network_handler::GetPropertiesCallback,
                     base::Bind(&MigrationTask::MigrateNetwork, this),
                     network_handler::ErrorCallback(),
                     service_path));
    }
  }

  void MigrateNetwork(const std::string& service_path,
                      const base::DictionaryValue& properties) {
    if (!cert_migrator_) {
      VLOG(2) << "NetworkCertMigrator already destroyed. Aborting migration.";
      return;
    }

    base::DictionaryValue new_properties;
    MigrateClientCertProperties(service_path, properties, &new_properties);
    MigrateNssProperties(service_path, properties, &new_properties);

    if (new_properties.empty())
      return;
    SendPropertiesToShill(service_path, new_properties);
  }

  void MigrateClientCertProperties(const std::string& service_path,
                                   const base::DictionaryValue& properties,
                                   base::DictionaryValue* new_properties) {
    int configured_slot_id = -1;
    std::string pkcs11_id;
    chromeos::client_cert::ConfigType config_type =
        chromeos::client_cert::CONFIG_TYPE_NONE;
    chromeos::client_cert::GetClientCertFromShillProperties(
        properties, &config_type, &configured_slot_id, &pkcs11_id);
    if (config_type == chromeos::client_cert::CONFIG_TYPE_NONE ||
        pkcs11_id.empty()) {
      return;
    }

    // OpenVPN configuration doesn't have a slot id to migrate.
    if (config_type == chromeos::client_cert::CONFIG_TYPE_OPENVPN)
      return;

    int real_slot_id = -1;
    scoped_refptr<net::X509Certificate> cert =
        FindCertificateWithPkcs11Id(pkcs11_id, &real_slot_id);
    if (!cert.get()) {
      LOG(WARNING) << "No matching cert found, removing the certificate "
                      "configuration from network " << service_path;
      chromeos::client_cert::SetEmptyShillProperties(config_type,
                                                     new_properties);
      return;
    }
    if (real_slot_id == -1) {
      LOG(WARNING) << "Found a certificate without slot id.";
      return;
    }

    if (cert.get() && real_slot_id != configured_slot_id) {
      VLOG(1) << "Network " << service_path
              << " is configured with no or an incorrect slot id.";
      chromeos::client_cert::SetShillProperties(
          config_type, real_slot_id, pkcs11_id, new_properties);
    }
  }

  void MigrateNssProperties(const std::string& service_path,
                            const base::DictionaryValue& properties,
                            base::DictionaryValue* new_properties) {
    std::string nss_key, pem_key, nickname;
    const base::ListValue* pem_property = NULL;
    UMANetworkType uma_type = UMA_NETWORK_TYPE_SIZE;

    GetNssAndPemProperties(
        properties, &nss_key, &pem_key, &pem_property, &nickname, &uma_type);
    if (nickname.empty())
      return;  // Didn't find any nickname.

    VLOG(2) << "Found NSS nickname to migrate. Property: " << nss_key
            << ", network: " << service_path;
    UMA_HISTOGRAM_ENUMERATION(
        "Network.MigrationNssToPem", uma_type, UMA_NETWORK_TYPE_SIZE);

    if (pem_property && !pem_property->empty()) {
      VLOG(2) << "PEM already exists, clearing NSS property.";
      ClearNssProperty(nss_key, new_properties);
      return;
    }

    scoped_refptr<net::X509Certificate> cert =
        FindCertificateWithNickname(nickname);
    if (!cert.get()) {
      VLOG(2) << "No matching cert found.";
      return;
    }

    std::string pem_encoded;
    if (!net::X509Certificate::GetPEMEncoded(cert->os_cert_handle(),
                                             &pem_encoded)) {
      LOG(ERROR) << "PEM encoding failed.";
      return;
    }

    ClearNssProperty(nss_key, new_properties);
    SetPemProperty(pem_key, pem_encoded, new_properties);
  }

  void GetNssAndPemProperties(const base::DictionaryValue& shill_properties,
                              std::string* nss_key,
                              std::string* pem_key,
                              const base::ListValue** pem_property,
                              std::string* nickname,
                              UMANetworkType* uma_type) {
    struct NssPem {
      const char* read_prefix;
      const char* nss_key;
      const char* pem_key;
      UMANetworkType uma_type;
    } const kNssPemMap[] = {
        { NULL, shill::kEapCaCertNssProperty, shill::kEapCaCertPemProperty,
         UMA_NETWORK_TYPE_EAP },
        { shill::kProviderProperty, shill::kL2tpIpsecCaCertNssProperty,
         shill::kL2tpIpsecCaCertPemProperty, UMA_NETWORK_TYPE_IPSEC },
        { shill::kProviderProperty, shill::kOpenVPNCaCertNSSProperty,
         shill::kOpenVPNCaCertPemProperty, UMA_NETWORK_TYPE_OPENVPN },
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNssPemMap); ++i) {
      const base::DictionaryValue* dict = &shill_properties;
      if (kNssPemMap[i].read_prefix) {
        shill_properties.GetDictionaryWithoutPathExpansion(
            kNssPemMap[i].read_prefix, &dict);
        if (!dict)
          continue;
      }
      dict->GetStringWithoutPathExpansion(kNssPemMap[i].nss_key, nickname);
      if (!nickname->empty()) {
        *nss_key = kNssPemMap[i].nss_key;
        *pem_key = kNssPemMap[i].pem_key;
        *uma_type = kNssPemMap[i].uma_type;
        dict->GetListWithoutPathExpansion(kNssPemMap[i].pem_key, pem_property);
        return;
      }
    }
  }

  void ClearNssProperty(const std::string& nss_key,
                        base::DictionaryValue* new_properties) {
    new_properties->SetStringWithoutPathExpansion(nss_key, std::string());
  }

  scoped_refptr<net::X509Certificate> FindCertificateWithPkcs11Id(
      const std::string& pkcs11_id, int* slot_id) {
    *slot_id = -1;
    for (net::CertificateList::iterator it = certs_.begin(); it != certs_.end();
         ++it) {
      int current_slot_id = -1;
      std::string current_pkcs11_id =
          CertLoader::GetPkcs11IdAndSlotForCert(**it, &current_slot_id);
      if (current_pkcs11_id == pkcs11_id) {
        *slot_id = current_slot_id;
        return *it;
      }
    }
    return NULL;
  }

  scoped_refptr<net::X509Certificate> FindCertificateWithNickname(
      const std::string& nickname) {
    for (net::CertificateList::iterator it = certs_.begin(); it != certs_.end();
         ++it) {
      if (nickname == GetNickname(**it))
        return *it;
    }
    return NULL;
  }

  void SetPemProperty(const std::string& pem_key,
                      const std::string& pem_encoded_cert,
                      base::DictionaryValue* new_properties) {
    scoped_ptr<base::ListValue> ca_cert_pems(new base::ListValue);
    ca_cert_pems->AppendString(pem_encoded_cert);
    new_properties->SetWithoutPathExpansion(pem_key, ca_cert_pems.release());
  }

  void SendPropertiesToShill(const std::string& service_path,
                             const base::DictionaryValue& properties) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperties(
        dbus::ObjectPath(service_path),
        properties,
        base::Bind(
            &MigrationTask::NotifyNetworkStateHandler, this, service_path),
        base::Bind(&MigrationTask::LogErrorAndNotifyNetworkStateHandler,
                   this,
                   service_path));
  }

  void LogErrorAndNotifyNetworkStateHandler(const std::string& service_path,
                                            const std::string& error_name,
                                            const std::string& error_message) {
    network_handler::ShillErrorCallbackFunction(
        "MigrationTask.SetProperties failed",
        service_path,
        network_handler::ErrorCallback(),
        error_name,
        error_message);
    NotifyNetworkStateHandler(service_path);
  }

  void NotifyNetworkStateHandler(const std::string& service_path) {
    if (!cert_migrator_) {
      VLOG(2) << "NetworkCertMigrator already destroyed. Aborting migration.";
      return;
    }
    cert_migrator_->network_state_handler_->RequestUpdateForNetwork(
        service_path);
  }

 private:
  friend class base::RefCounted<MigrationTask>;
  virtual ~MigrationTask() {
  }

  net::CertificateList certs_;
  base::WeakPtr<NetworkCertMigrator> cert_migrator_;
};

NetworkCertMigrator::NetworkCertMigrator()
    : network_state_handler_(NULL),
      weak_ptr_factory_(this) {
}

NetworkCertMigrator::~NetworkCertMigrator() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (CertLoader::IsInitialized())
    CertLoader::Get()->RemoveObserver(this);
}

void NetworkCertMigrator::Init(NetworkStateHandler* network_state_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  DCHECK(CertLoader::IsInitialized());
  CertLoader::Get()->AddObserver(this);
}

void NetworkCertMigrator::NetworkListChanged() {
  if (!CertLoader::Get()->certificates_loaded()) {
    VLOG(2) << "Certs not loaded yet.";
    return;
  }
  // Run the migration process from deprecated CaCertNssProperties to CaCertPem
  // and to fix missing or incorrect slot ids of client certificates.
  VLOG(2) << "Start certificate migration of network configurations.";
  scoped_refptr<MigrationTask> helper(new MigrationTask(
      CertLoader::Get()->cert_list(), weak_ptr_factory_.GetWeakPtr()));
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true,  // only configured networks
      false, // visible and not visible networks
      0,     // no count limit
      &networks);
  helper->Run(networks);
}

void NetworkCertMigrator::OnCertificatesLoaded(
    const net::CertificateList& cert_list,
    bool initial_load) {
  // Maybe there are networks referring to certs that were not loaded before but
  // are now.
  NetworkListChanged();
}

}  // namespace chromeos
