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

// Checks which of the given |networks| has one of the deprecated
// CaCertNssProperties set. If such a network already has a CaCertPEM property,
// then the NssProperty is cleared. Otherwise, the NssProperty is compared with
// the nickname of each certificate of |certs|. If a match is found, then the
// CaCertPemProperty is set and the NssProperty is cleared. Otherwise, the
// network is not modified.
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
    // according to the NetworkStateHandler.
    for (NetworkStateHandler::NetworkStateList::const_iterator it =
             networks.begin(); it != networks.end(); ++it) {
      if (!(*it)->HasCACertNSS())
        continue;
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
      ClearNssProperty(service_path, nss_key);
      return;
    }

    scoped_refptr<net::X509Certificate> cert =
        FindCertificateWithNickname(nickname);
    if (!cert) {
      VLOG(2) << "No matching cert found.";
      return;
    }

    std::string pem_encoded;
    if (!net::X509Certificate::GetPEMEncoded(cert->os_cert_handle(),
                                             &pem_encoded)) {
      LOG(ERROR) << "PEM encoding failed.";
      return;
    }

    SetNssAndPemProperties(service_path, nss_key, pem_key, pem_encoded);
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

  void ClearNssProperty(const std::string& service_path,
                        const std::string& nss_key) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(service_path),
        nss_key,
        base::StringValue(std::string()),
        base::Bind(
            &MigrationTask::NotifyNetworkStateHandler, this, service_path),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "MigrationTask.SetProperty failed",
                   service_path,
                   network_handler::ErrorCallback()));
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

  void SetNssAndPemProperties(const std::string& service_path,
                              const std::string& nss_key,
                              const std::string& pem_key,
                              const std::string& pem_encoded_cert) {
    base::DictionaryValue new_properties;
    new_properties.SetStringWithoutPathExpansion(nss_key, std::string());
    scoped_ptr<base::ListValue> ca_cert_pems(new base::ListValue);
    ca_cert_pems->AppendString(pem_encoded_cert);
    new_properties.SetWithoutPathExpansion(pem_key, ca_cert_pems.release());

    DBusThreadManager::Get()->GetShillServiceClient()->SetProperties(
        dbus::ObjectPath(service_path),
        new_properties,
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
  // Run the migration process from deprecated CaCertNssProperties to CaCertPem.
  VLOG(2) << "Start NSS nickname to PEM migration.";
  scoped_refptr<MigrationTask> helper(new MigrationTask(
      CertLoader::Get()->cert_list(), weak_ptr_factory_.GetWeakPtr()));
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetVisibleNetworkList(&networks);
  helper->Run(networks);
}

void NetworkCertMigrator::OnCertificatesLoaded(
    const net::CertificateList& cert_list,
    bool initial_load) {
  // Maybe there are networks referring to certs (by NSS nickname) that were not
  // loaded before but are now.
  NetworkListChanged();
}

}  // namespace chromeos
