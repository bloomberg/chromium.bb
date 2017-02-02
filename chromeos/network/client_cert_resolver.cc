// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <certt.h>  // for (SECCertUsageEnum) certUsageAnyCA
#include <pk11pub.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Describes a network |network_path| for which a matching certificate |cert_id|
// was found or for which no certificate was found (|cert_id| will be empty).
struct ClientCertResolver::NetworkAndMatchingCert {
  NetworkAndMatchingCert(const std::string& network_path,
                         client_cert::ConfigType config_type,
                         const std::string& cert_id,
                         int slot_id,
                         const std::string& configured_identity)
      : service_path(network_path),
        cert_config_type(config_type),
        pkcs11_id(cert_id),
        key_slot_id(slot_id),
        identity(configured_identity) {}

  std::string service_path;
  client_cert::ConfigType cert_config_type;

  // The id of the matching certificate or empty if no certificate was found.
  std::string pkcs11_id;

  // The id of the slot containing the certificate and the private key.
  int key_slot_id;

  // The ONC WiFi.EAP.Identity field can contain variables like
  // ${CERT_SAN_EMAIL} which are expanded by ClientCertResolver.
  // |identity| stores a copy of this string after the substitution
  // has been done.
  std::string identity;
};

typedef std::vector<ClientCertResolver::NetworkAndMatchingCert>
    NetworkCertMatches;

namespace {

// Returns true if |vector| contains |value|.
template <class T>
bool ContainsValue(const std::vector<T>& vector, const T& value) {
  return find(vector.begin(), vector.end(), value) != vector.end();
}

// Returns true if a private key for certificate |cert| is installed.
bool HasPrivateKey(const net::X509Certificate& cert) {
  PK11SlotInfo* slot =
      PK11_KeyForCertExists(cert.os_cert_handle(), nullptr, nullptr);
  if (!slot)
    return false;

  PK11_FreeSlot(slot);
  return true;
}

// Describes a certificate which is issued by |issuer| (encoded as PEM).
// |issuer| can be empty if no issuer certificate is found in the database.
struct CertAndIssuer {
  CertAndIssuer(const scoped_refptr<net::X509Certificate>& certificate,
                const std::string& issuer)
      : cert(certificate),
        pem_encoded_issuer(issuer) {}

  scoped_refptr<net::X509Certificate> cert;
  std::string pem_encoded_issuer;
};

bool CompareCertExpiration(const CertAndIssuer& a,
                           const CertAndIssuer& b) {
  return (a.cert->valid_expiry() > b.cert->valid_expiry());
}

// Describes a network that is configured with the certificate pattern
// |client_cert_pattern|.
struct NetworkAndCertPattern {
  NetworkAndCertPattern(const std::string& network_path,
                        const client_cert::ClientCertConfig& client_cert_config)
      : service_path(network_path),
        cert_config(client_cert_config) {}

  std::string service_path;
  client_cert::ClientCertConfig cert_config;
};

// A unary predicate that returns true if the given CertAndIssuer matches the
// given certificate pattern.
struct MatchCertWithPattern {
  explicit MatchCertWithPattern(const CertificatePattern& cert_pattern)
      : pattern(cert_pattern) {}

  bool operator()(const CertAndIssuer& cert_and_issuer) {
    if (!pattern.issuer().Empty() &&
        !client_cert::CertPrincipalMatches(pattern.issuer(),
                                           cert_and_issuer.cert->issuer())) {
      return false;
    }
    if (!pattern.subject().Empty() &&
        !client_cert::CertPrincipalMatches(pattern.subject(),
                                           cert_and_issuer.cert->subject())) {
      return false;
    }

    const std::vector<std::string>& issuer_ca_pems = pattern.issuer_ca_pems();
    if (!issuer_ca_pems.empty() &&
        !ContainsValue(issuer_ca_pems, cert_and_issuer.pem_encoded_issuer)) {
      return false;
    }
    return true;
  }

  const CertificatePattern pattern;
};

// Lookup the issuer certificate of |cert|. If it is available, return the PEM
// encoding of that certificate. Otherwise return the empty string.
std::string GetPEMEncodedIssuer(const net::X509Certificate& cert) {
  net::ScopedCERTCertificate issuer_handle(
      CERT_FindCertIssuer(cert.os_cert_handle(), PR_Now(), certUsageAnyCA));
  if (!issuer_handle) {
    VLOG(1) << "Couldn't find an issuer.";
    return std::string();
  }

  scoped_refptr<net::X509Certificate> issuer =
      net::X509Certificate::CreateFromHandle(
          issuer_handle.get(),
          net::X509Certificate::OSCertHandles() /* no intermediate certs */);
  if (!issuer.get()) {
    LOG(ERROR) << "Couldn't create issuer cert.";
    return std::string();
  }
  std::string pem_encoded_issuer;
  if (!net::X509Certificate::GetPEMEncoded(issuer->os_cert_handle(),
                                           &pem_encoded_issuer)) {
    LOG(ERROR) << "Couldn't PEM-encode certificate.";
    return std::string();
  }
  return pem_encoded_issuer;
}

std::vector<CertAndIssuer> CreateSortedCertAndIssuerList(
    const net::CertificateList& certs,
    base::Time now) {
  // Filter all client certs and determines each certificate's issuer, which is
  // required for the pattern matching.
  std::vector<CertAndIssuer> client_certs;
  for (net::CertificateList::const_iterator it = certs.begin();
       it != certs.end(); ++it) {
    const net::X509Certificate& cert = **it;
    if (cert.valid_expiry().is_null() || now > cert.valid_expiry() ||
        !HasPrivateKey(cert) ||
        !CertLoader::IsCertificateHardwareBacked(&cert)) {
      continue;
    }
    client_certs.push_back(CertAndIssuer(*it, GetPEMEncodedIssuer(cert)));
  }

  std::sort(client_certs.begin(), client_certs.end(), &CompareCertExpiration);
  return client_certs;
}

// Searches for matches between |networks| and |certs| and writes matches to
// |matches|. Because this calls NSS functions and is potentially slow, it must
// be run on a worker thread.
void FindCertificateMatches(const net::CertificateList& certs,
                            std::vector<NetworkAndCertPattern>* networks,
                            base::Time now,
                            NetworkCertMatches* matches) {
  std::vector<CertAndIssuer> client_certs(
      CreateSortedCertAndIssuerList(certs, now));

  for (std::vector<NetworkAndCertPattern>::const_iterator it =
           networks->begin();
       it != networks->end(); ++it) {
    std::vector<CertAndIssuer>::iterator cert_it =
        std::find_if(client_certs.begin(),
                     client_certs.end(),
                     MatchCertWithPattern(it->cert_config.pattern));
    std::string pkcs11_id;
    int slot_id = -1;
    std::string identity;

    if (cert_it == client_certs.end()) {
      VLOG(1) << "Couldn't find a matching client cert for network "
              << it->service_path;
      // Leave |pkcs11_id| empty to indicate that no cert was found for this
      // network.
    } else {
      pkcs11_id =
          CertLoader::GetPkcs11IdAndSlotForCert(*cert_it->cert, &slot_id);
      if (pkcs11_id.empty()) {
        LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
        // So far this error is not expected to happen. We can just continue, in
        // the worst case the user can remove the problematic cert.
        continue;
      }

      // If the policy specifies an identity containing ${CERT_SAN_xxx},
      // see if the cert contains a suitable subjectAltName that can be
      // stuffed into the shill properties.
      identity = it->cert_config.policy_identity;
      std::vector<std::string> names;

      size_t offset = identity.find(onc::substitutes::kCertSANEmail, 0);
      if (offset != std::string::npos) {
        std::vector<std::string> names;
        net::x509_util::GetRFC822SubjectAltNames(
            cert_it->cert->os_cert_handle(), &names);
        if (!names.empty()) {
          base::ReplaceSubstringsAfterOffset(
              &identity, offset, onc::substitutes::kCertSANEmail, names[0]);
        }
      }

      offset = identity.find(onc::substitutes::kCertSANUPN, 0);
      if (offset != std::string::npos) {
        std::vector<std::string> names;
        net::x509_util::GetUPNSubjectAltNames(cert_it->cert->os_cert_handle(),
                                              &names);
        if (!names.empty()) {
          base::ReplaceSubstringsAfterOffset(
              &identity, offset, onc::substitutes::kCertSANUPN, names[0]);
        }
      }
    }
    matches->push_back(ClientCertResolver::NetworkAndMatchingCert(
        it->service_path, it->cert_config.location, pkcs11_id, slot_id,
        identity));
  }
}

void LogError(const std::string& service_path,
              const std::string& dbus_error_name,
              const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "ClientCertResolver.SetProperties failed",
      service_path,
      network_handler::ErrorCallback(),
      dbus_error_name,
      dbus_error_message);
}

bool ClientCertificatesLoaded() {
  if (!CertLoader::Get()->certificates_loaded()) {
    VLOG(1) << "Certificates not loaded yet.";
    return false;
  }
  return true;
}

}  // namespace

ClientCertResolver::ClientCertResolver()
    : resolve_task_running_(false),
      network_properties_changed_(false),
      network_state_handler_(nullptr),
      managed_network_config_handler_(nullptr),
      testing_clock_(nullptr),
      weak_ptr_factory_(this) {}

ClientCertResolver::~ClientCertResolver() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (CertLoader::IsInitialized())
    CertLoader::Get()->RemoveObserver(this);
  if (managed_network_config_handler_)
    managed_network_config_handler_->RemoveObserver(this);
}

void ClientCertResolver::Init(
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_config_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  DCHECK(managed_network_config_handler);
  managed_network_config_handler_ = managed_network_config_handler;
  managed_network_config_handler_->AddObserver(this);

  CertLoader::Get()->AddObserver(this);
}

void ClientCertResolver::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ClientCertResolver::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ClientCertResolver::IsAnyResolveTaskRunning() const {
  return resolve_task_running_;
}

// static
bool ClientCertResolver::ResolveCertificatePatternSync(
    const client_cert::ConfigType client_cert_type,
    const CertificatePattern& pattern,
    base::DictionaryValue* shill_properties) {
  // Prepare and sort the list of known client certs.
  std::vector<CertAndIssuer> client_certs(CreateSortedCertAndIssuerList(
      CertLoader::Get()->cert_list(), base::Time::Now()));

  // Search for a certificate matching the pattern.
  std::vector<CertAndIssuer>::iterator cert_it = std::find_if(
      client_certs.begin(), client_certs.end(), MatchCertWithPattern(pattern));

  if (cert_it == client_certs.end()) {
    VLOG(1) << "Couldn't find a matching client cert";
    client_cert::SetEmptyShillProperties(client_cert_type, shill_properties);
    return false;
  }

  int slot_id = -1;
  std::string pkcs11_id =
      CertLoader::GetPkcs11IdAndSlotForCert(*cert_it->cert, &slot_id);
  if (pkcs11_id.empty()) {
    LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
    // So far this error is not expected to happen. We can just continue, in
    // the worst case the user can remove the problematic cert.
    return false;
  }
  client_cert::SetShillProperties(
      client_cert_type, slot_id, pkcs11_id, shill_properties);
  return true;
}

void ClientCertResolver::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ClientCertResolver::NetworkListChanged() {
  VLOG(2) << "NetworkListChanged.";
  if (!ClientCertificatesLoaded())
    return;
  // Configure only networks that were not configured before.

  // We'll drop networks from |resolved_networks_|, which are not known anymore.
  std::set<std::string> old_resolved_networks;
  old_resolved_networks.swap(resolved_networks_);

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);

  NetworkStateHandler::NetworkStateList networks_to_check;
  for (NetworkStateHandler::NetworkStateList::const_iterator it =
           networks.begin(); it != networks.end(); ++it) {
    const std::string& service_path = (*it)->path();
    if (base::ContainsKey(old_resolved_networks, service_path)) {
      resolved_networks_.insert(service_path);
      continue;
    }
    networks_to_check.push_back(*it);
  }

  ResolveNetworks(networks_to_check);
}

void ClientCertResolver::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!ClientCertificatesLoaded())
    return;
  if (!network->IsConnectedState() && !network->IsConnectingState())
    ResolveNetworks(NetworkStateHandler::NetworkStateList(1, network));
}

void ClientCertResolver::OnCertificatesLoaded(
    const net::CertificateList& cert_list,
    bool initial_load) {
  VLOG(2) << "OnCertificatesLoaded.";
  if (!ClientCertificatesLoaded())
    return;
  // Compare all networks with all certificates.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);
  ResolveNetworks(networks);
}

void ClientCertResolver::PolicyAppliedToNetwork(
    const std::string& service_path) {
  VLOG(2) << "PolicyAppliedToNetwork " << service_path;
  if (!ClientCertificatesLoaded())
    return;
  // Compare this network with all certificates.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          service_path, true /* configured_only */);
  if (!network) {
    LOG(ERROR) << "service path '" << service_path << "' unknown.";
    return;
  }
  NetworkStateHandler::NetworkStateList networks;
  networks.push_back(network);
  ResolveNetworks(networks);
}

void ClientCertResolver::ResolveNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  std::unique_ptr<std::vector<NetworkAndCertPattern>> networks_to_resolve(
      new std::vector<NetworkAndCertPattern>);

  // Filter networks with ClientCertPattern. As ClientCertPatterns can only be
  // set by policy, we check there.
  for (NetworkStateHandler::NetworkStateList::const_iterator it =
           networks.begin(); it != networks.end(); ++it) {
    const NetworkState* network = *it;

    // In any case, don't check this network again in NetworkListChanged.
    resolved_networks_.insert(network->path());

    // If this network is not configured, it cannot have a ClientCertPattern.
    if (network->profile_path().empty())
      continue;

    const base::DictionaryValue* policy =
        managed_network_config_handler_->FindPolicyByGuidAndProfile(
            network->guid(), network->profile_path());

    if (!policy) {
      VLOG(1) << "The policy for network " << network->path() << " with GUID "
              << network->guid() << " is not available yet.";
      // Skip this network for now. Once the policy is loaded, PolicyApplied()
      // will retry.
      continue;
    }

    VLOG(2) << "Inspecting network " << network->path();
    client_cert::ClientCertConfig cert_config;
    OncToClientCertConfig(*policy, &cert_config);

    // Skip networks that don't have a ClientCertPattern.
    if (cert_config.client_cert_type != ::onc::client_cert::kPattern)
      continue;

    networks_to_resolve->push_back(
        NetworkAndCertPattern(network->path(), cert_config));
  }

  if (networks_to_resolve->empty()) {
    VLOG(1) << "No networks to resolve.";
    NotifyResolveRequestCompleted();
    return;
  }

  if (resolve_task_running_) {
    VLOG(1) << "A resolve task is already running. Queue this request.";
    for (const NetworkAndCertPattern& network_and_pattern :
         *networks_to_resolve) {
      queued_networks_to_resolve_.insert(network_and_pattern.service_path);
    }
    return;
  }

  VLOG(2) << "Start task for resolving client cert patterns.";
  resolve_task_running_ = true;
  NetworkCertMatches* matches = new NetworkCertMatches;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, base::TaskTraits()
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                     .MayBlock(),
      base::Bind(&FindCertificateMatches, CertLoader::Get()->cert_list(),
                 base::Owned(networks_to_resolve.release()), Now(), matches),
      base::Bind(&ClientCertResolver::ConfigureCertificates,
                 weak_ptr_factory_.GetWeakPtr(), base::Owned(matches)));
}

void ClientCertResolver::ResolvePendingNetworks() {
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               true /* configured_only */,
                                               false /* visible_only */,
                                               0 /* no limit */,
                                               &networks);

  NetworkStateHandler::NetworkStateList networks_to_resolve;
  for (const NetworkState* network : networks) {
    if (queued_networks_to_resolve_.count(network->path()) > 0)
      networks_to_resolve.push_back(network);
  }
  VLOG(1) << "Resolve pending " << networks_to_resolve.size() << " networks.";
  queued_networks_to_resolve_.clear();
  ResolveNetworks(networks_to_resolve);
}

void ClientCertResolver::ConfigureCertificates(NetworkCertMatches* matches) {
  for (NetworkCertMatches::const_iterator it = matches->begin();
       it != matches->end(); ++it) {
    VLOG(1) << "Configuring certificate of network " << it->service_path;
    base::DictionaryValue shill_properties;
    if (it->pkcs11_id.empty()) {
      client_cert::SetEmptyShillProperties(it->cert_config_type,
                                           &shill_properties);
    } else {
      client_cert::SetShillProperties(it->cert_config_type,
                                      it->key_slot_id,
                                      it->pkcs11_id,
                                      &shill_properties);
      if (!it->identity.empty()) {
        shill_properties.SetStringWithoutPathExpansion(
            shill::kEapIdentityProperty, it->identity);
      }
    }
    network_properties_changed_ = true;
    DBusThreadManager::Get()->GetShillServiceClient()->
        SetProperties(dbus::ObjectPath(it->service_path),
                        shill_properties,
                        base::Bind(&base::DoNothing),
                        base::Bind(&LogError, it->service_path));
    network_state_handler_->RequestUpdateForNetwork(it->service_path);
  }
  if (queued_networks_to_resolve_.empty())
    NotifyResolveRequestCompleted();
  else
    ResolvePendingNetworks();
}

void ClientCertResolver::NotifyResolveRequestCompleted() {
  VLOG(2) << "Notify observers: " << (network_properties_changed_ ? "" : "no ")
          << "networks changed.";
  resolve_task_running_ = false;
  const bool changed = network_properties_changed_;
  network_properties_changed_ = false;
  for (auto& observer : observers_)
    observer.ResolveRequestCompleted(changed);
}

base::Time ClientCertResolver::Now() const {
  if (testing_clock_)
    return testing_clock_->Now();
  return base::Time::Now();
}

}  // namespace chromeos
