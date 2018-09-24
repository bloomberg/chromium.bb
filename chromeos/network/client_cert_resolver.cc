// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <certt.h>  // for (SECCertUsageEnum) certUsageAnyCA
#include <pk11pub.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/certificate_helper.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/tools/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "dbus/object_path.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Describes a resolved client certificate along with the EAP identity field.
struct MatchingCert {
  MatchingCert() {}

  MatchingCert(const std::string& pkcs11_id,
               int key_slot_id,
               const std::string& configured_identity)
      : pkcs11_id(pkcs11_id),
        key_slot_id(key_slot_id),
        identity(configured_identity) {}

  bool operator==(const MatchingCert& other) const {
    return pkcs11_id == other.pkcs11_id && key_slot_id == other.key_slot_id &&
           identity == other.identity;
  }

  // The id of the matching certificate.
  std::string pkcs11_id;

  // The id of the slot containing the certificate and the private key.
  int key_slot_id = -1;

  // The ONC WiFi.EAP.Identity field can contain variables like
  // ${CERT_SAN_EMAIL} which are expanded by ClientCertResolver.
  // |identity| stores a copy of this string after the substitution
  // has been done.
  std::string identity;
};

// Describes a network that is configured with |client_cert_config|, which
// includes the certificate pattern.
struct NetworkAndCertPattern {
  NetworkAndCertPattern(const std::string& network_path,
                        const client_cert::ClientCertConfig& client_cert_config)
      : service_path(network_path), cert_config(client_cert_config) {}

  std::string service_path;
  client_cert::ClientCertConfig cert_config;
};

// The certificate resolving status of a known network that needs certificate
// pattern resolution.
enum class ResolveStatus { kResolving, kResolved };

// Returns substitutions based on |cert|'s contents to be used in a
// VariableExpander.
std::map<std::string, std::string> GetSubstitutionsForCert(
    CERTCertificate* cert) {
  std::map<std::string, std::string> substitutions;

  {
    std::vector<std::string> names;
    net::x509_util::GetRFC822SubjectAltNames(cert, &names);
    // Currently, we only use the first specified RFC8222
    // SubjectAlternativeName.
    std::string firstSANEmail;
    if (!names.empty())
      firstSANEmail = names[0];
    substitutions[onc::substitutes::kCertSANEmail] = firstSANEmail;
  }

  {
    std::vector<std::string> names;
    net::x509_util::GetUPNSubjectAltNames(cert, &names);
    // Currently, we only use the first specified UPN SubjectAlternativeName.
    std::string firstSANUPN;
    if (!names.empty())
      firstSANUPN = names[0];
    substitutions[onc::substitutes::kCertSANUPN] = firstSANUPN;
  }

  substitutions[onc::substitutes::kCertSubjectCommonName] =
      certificate::GetCertAsciiSubjectCommonName(cert);

  return substitutions;
}

}  // namespace

namespace internal {

// Describes the resolve status for a network, and if resolving already
// completed, also holds the matched certificate.
struct MatchingCertAndResolveStatus {
  // kResolving if client cert resolution is pending, kResolved if client cert
  // resolution has been completed for the network.
  ResolveStatus resolve_status = ResolveStatus::kResolving;

  // This is set to the last resolved client certificate or nullopt if no
  // matching certificate has been found when |resolve_status| is kResolved.
  // This is also used to determine if re-resolving a network actually changed
  // any properties.
  base::Optional<MatchingCert> matching_cert;
};

// Describes a network |network_path| and the client cert resolution result.
struct NetworkAndMatchingCert {
  NetworkAndMatchingCert(const NetworkAndCertPattern& network_and_pattern,
                         base::Optional<MatchingCert> matching_cert)
      : service_path(network_and_pattern.service_path),
        cert_config_type(network_and_pattern.cert_config.location),
        matching_cert(matching_cert) {}

  std::string service_path;
  client_cert::ConfigType cert_config_type;

  // The resolved certificate, or |nullopt| if no matching certificate has been
  // found.
  base::Optional<MatchingCert> matching_cert;
};

}  // namespace internal

using internal::MatchingCertAndResolveStatus;
using internal::NetworkAndMatchingCert;

namespace {

// Returns true if a private key for certificate |cert| is installed.
// Note that HasPrivateKey is not a cheap operation: it iterates all tokens and
// attempts to look up the private key.
bool HasPrivateKey(CERTCertificate* cert) {
  PK11SlotInfo* slot = PK11_KeyForCertExists(cert, nullptr, nullptr);
  if (!slot)
    return false;

  PK11_FreeSlot(slot);
  return true;
}

// Describes a certificate which is issued by |issuer| (encoded as PEM).
// |issuer| can be empty if no issuer certificate is found in the database.
struct CertAndIssuer {
  CertAndIssuer(net::ScopedCERTCertificate certificate,
                const std::string& issuer)
      : cert(std::move(certificate)), pem_encoded_issuer(issuer) {}

  net::ScopedCERTCertificate cert;
  std::string pem_encoded_issuer;
};

bool CompareCertExpiration(const CertAndIssuer& a, const CertAndIssuer& b) {
  base::Time a_not_after;
  base::Time b_not_after;
  net::x509_util::GetValidityTimes(a.cert.get(), nullptr, &a_not_after);
  net::x509_util::GetValidityTimes(b.cert.get(), nullptr, &b_not_after);
  return a_not_after > b_not_after;
}

// A unary predicate that returns true if the given CertAndIssuer matches the
// given certificate pattern.
struct MatchCertWithPattern {
  explicit MatchCertWithPattern(const CertificatePattern& cert_pattern)
      : pattern(cert_pattern) {}

  bool operator()(const CertAndIssuer& cert_and_issuer) {
    if (!pattern.issuer().Empty() || !pattern.subject().Empty()) {
      // Allow UTF-8 inside PrintableStrings in client certificates. See
      // crbug.com/770323 and crbug.com/788655.
      net::X509Certificate::UnsafeCreateOptions options;
      options.printable_string_is_utf8 = true;
      scoped_refptr<net::X509Certificate> x509_cert =
          net::x509_util::CreateX509CertificateFromCERTCertificate(
              cert_and_issuer.cert.get(), {}, options);
      if (!x509_cert)
        return false;
      if (!pattern.issuer().Empty() &&
          !client_cert::CertPrincipalMatches(pattern.issuer(),
                                             x509_cert->issuer())) {
        return false;
      }
      if (!pattern.subject().Empty() &&
          !client_cert::CertPrincipalMatches(pattern.subject(),
                                             x509_cert->subject())) {
        return false;
      }
    }

    const std::vector<std::string>& issuer_ca_pems = pattern.issuer_ca_pems();
    if (!issuer_ca_pems.empty() &&
        !base::ContainsValue(issuer_ca_pems,
                             cert_and_issuer.pem_encoded_issuer)) {
      return false;
    }
    return true;
  }

  const CertificatePattern pattern;
};

// Lookup the issuer certificate of |cert|. If it is available, return the PEM
// encoding of that certificate. Otherwise return the empty string.
std::string GetPEMEncodedIssuer(CERTCertificate* cert) {
  net::ScopedCERTCertificate issuer_handle(
      CERT_FindCertIssuer(cert, PR_Now(), certUsageAnyCA));
  if (!issuer_handle) {
    VLOG(1) << "Couldn't find an issuer.";
    return std::string();
  }

  scoped_refptr<net::X509Certificate> issuer =
      net::x509_util::CreateX509CertificateFromCERTCertificate(
          issuer_handle.get());
  if (!issuer.get()) {
    LOG(ERROR) << "Couldn't create issuer cert.";
    return std::string();
  }
  std::string pem_encoded_issuer;
  if (!net::X509Certificate::GetPEMEncoded(issuer->cert_buffer(),
                                           &pem_encoded_issuer)) {
    LOG(ERROR) << "Couldn't PEM-encode certificate.";
    return std::string();
  }
  return pem_encoded_issuer;
}

std::vector<CertAndIssuer> CreateSortedCertAndIssuerList(
    net::ScopedCERTCertificateList certs,
    base::Time now) {
  // Filter all client certs and determines each certificate's issuer, which is
  // required for the pattern matching.
  // TODO(pmarko): Consider moving the filtering of client certs into
  // CertLoader. It should not be in ClientCertResolver's responsibility to
  // decide if a certificate is a valid client certificate or not. Other
  // consumers of CertLoader could also use a pre-filtered list (e.g.
  // NetworkCertMigrator). See crbug.com/781693.
  std::vector<CertAndIssuer> client_certs;
  for (net::ScopedCERTCertificate& scoped_cert : certs) {
    CERTCertificate* cert = scoped_cert.get();
    base::Time not_after;
    // HasPrivateKey should be invoked after IsCertificateHardwareBacked for
    // performance reasons.
    if (!net::x509_util::GetValidityTimes(cert, nullptr, &not_after) ||
        now > not_after || !CertLoader::IsCertificateHardwareBacked(cert) ||
        !HasPrivateKey(cert)) {
      continue;
    }
    std::string pem_encoded_issuer = GetPEMEncodedIssuer(cert);
    client_certs.push_back(
        CertAndIssuer(std::move(scoped_cert), pem_encoded_issuer));
  }

  std::sort(client_certs.begin(), client_certs.end(), &CompareCertExpiration);
  return client_certs;
}

// Searches for matches between |networks| and |all_certs| (for networks
// configured in user policy) / |system_token_client_certs| (for networks
// configured in device policy). Returns the matches that were found. Because
// this calls NSS functions and is potentially slow, it must be run on a worker
// thread.
std::vector<NetworkAndMatchingCert> FindCertificateMatches(
    net::ScopedCERTCertificateList all_certs,
    net::ScopedCERTCertificateList system_token_client_certs,
    const std::vector<NetworkAndCertPattern>& networks,
    base::Time now) {
  std::vector<NetworkAndMatchingCert> matches;

  std::vector<CertAndIssuer> all_client_certs(
      CreateSortedCertAndIssuerList(std::move(all_certs), now));
  std::vector<CertAndIssuer> system_client_certs(
      CreateSortedCertAndIssuerList(std::move(system_token_client_certs), now));

  for (const NetworkAndCertPattern& network_and_pattern : networks) {
    // Use only certs from the system token if the source of the client cert
    // pattern is device policy.
    std::vector<CertAndIssuer>* client_certs =
        network_and_pattern.cert_config.onc_source ==
                ::onc::ONC_SOURCE_DEVICE_POLICY
            ? &system_client_certs
            : &all_client_certs;
    auto cert_it = std::find_if(
        client_certs->begin(), client_certs->end(),
        MatchCertWithPattern(network_and_pattern.cert_config.pattern));
    if (cert_it == client_certs->end()) {
      VLOG(1) << "Couldn't find a matching client cert for network "
              << network_and_pattern.service_path;
      matches.push_back(
          NetworkAndMatchingCert(network_and_pattern, base::nullopt));
      continue;
    }

    std::string pkcs11_id;
    int slot_id = -1;

    pkcs11_id =
        CertLoader::GetPkcs11IdAndSlotForCert(cert_it->cert.get(), &slot_id);
    if (pkcs11_id.empty()) {
      LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
      // So far this error is not expected to happen. We can just continue, in
      // the worst case the user can remove the problematic cert.
      continue;
    }

    // Expand placeholders in the identity string that are specific to the
    // client certificate.
    VariableExpander variable_expander(
        GetSubstitutionsForCert(cert_it->cert.get()));
    std::string identity = network_and_pattern.cert_config.policy_identity;
    const bool success = variable_expander.ExpandString(&identity);
    LOG_IF(ERROR, !success)
        << "Error during variable expansion in ONC-configured identity";

    matches.push_back(NetworkAndMatchingCert(
        network_and_pattern, MatchingCert(pkcs11_id, slot_id, identity)));
  }
  return matches;
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
  if (!CertLoader::Get()->initial_load_finished()) {
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
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ClientCertResolver::~ClientCertResolver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  DCHECK(managed_network_config_handler);
  managed_network_config_handler_ = managed_network_config_handler;
  managed_network_config_handler_->AddObserver(this);

  CertLoader::Get()->AddObserver(this);
}

void ClientCertResolver::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ClientCertResolver::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool ClientCertResolver::IsAnyResolveTaskRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resolve_task_running_;
}

// static
bool ClientCertResolver::ResolveCertificatePatternSync(
    const client_cert::ConfigType client_cert_type,
    const client_cert::ClientCertConfig& client_cert_config,
    base::DictionaryValue* shill_properties) {
  // Prepare and sort the list of known client certs. Use only certs from the
  // system token if the source of the client cert pattern is device policy.
  std::vector<CertAndIssuer> client_certs;
  if (client_cert_config.onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY) {
    client_certs = CreateSortedCertAndIssuerList(
        net::x509_util::DupCERTCertificateList(
            CertLoader::Get()->system_token_client_certs()),
        base::Time::Now());
  } else {
    client_certs = CreateSortedCertAndIssuerList(
        net::x509_util::DupCERTCertificateList(CertLoader::Get()->all_certs()),
        base::Time::Now());
  }

  // Search for a certificate matching the pattern.
  std::vector<CertAndIssuer>::iterator cert_it =
      std::find_if(client_certs.begin(), client_certs.end(),
                   MatchCertWithPattern(client_cert_config.pattern));

  if (cert_it == client_certs.end()) {
    VLOG(1) << "Couldn't find a matching client cert";
    client_cert::SetEmptyShillProperties(client_cert_type, shill_properties);
    return false;
  }

  int slot_id = -1;
  std::string pkcs11_id =
      CertLoader::GetPkcs11IdAndSlotForCert(cert_it->cert.get(), &slot_id);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "NetworkListChanged.";
  if (!ClientCertificatesLoaded())
    return;
  // Configure only networks that were not configured before.

  // We'll drop networks from |networks_status_|, which are not known anymore.
  base::flat_map<std::string, MatchingCertAndResolveStatus> old_networks_status;
  old_networks_status.swap(networks_status_);

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);

  NetworkStateHandler::NetworkStateList networks_to_check;
  for (const NetworkState* network : networks) {
    const std::string& service_path = network->path();
    auto old_networks_status_iter = old_networks_status.find(service_path);
    if (old_networks_status_iter != old_networks_status.end()) {
      networks_status_[service_path] = old_networks_status_iter->second;
      continue;
    }
    networks_to_check.push_back(network);
  }

  if (!networks_to_check.empty()) {
    NET_LOG(EVENT) << "ClientCertResolver: NetworkListChanged: "
                   << networks_to_check.size();
    ResolveNetworks(networks_to_check);
  }
}

void ClientCertResolver::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ClientCertificatesLoaded())
    return;
  if (!network->IsConnectingOrConnected()) {
    NET_LOG(EVENT) << "ClientCertResolver: ConnectionStateChanged: "
                   << network->name();
    ResolveNetworks(NetworkStateHandler::NetworkStateList(1, network));
  }
}

void ClientCertResolver::OnCertificatesLoaded(
    const net::ScopedCERTCertificateList& cert_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "OnCertificatesLoaded.";
  if (!ClientCertificatesLoaded())
    return;
  NET_LOG(EVENT) << "ClientCertResolver: Certificates Loaded.";
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  NET_LOG(EVENT) << "ClientCertResolver: PolicyAppliedToNetwork: "
                 << network->name();
  NetworkStateHandler::NetworkStateList networks;
  networks.push_back(network);
  ResolveNetworks(networks);
}

void ClientCertResolver::ResolveNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<NetworkAndCertPattern> networks_to_resolve;

  // Filter networks with ClientCertPattern. As ClientCertPatterns can only be
  // set by policy, we check there.
  for (const NetworkState* network : networks) {
    // If the network was not known before, mark it as known but with resolving
    // pending.
    if (networks_status_.find(network->path()) == networks_status_.end())
      networks_status_.insert_or_assign(network->path(),
                                        MatchingCertAndResolveStatus());

    // If this network is not configured, it cannot have a ClientCertPattern.
    if (network->profile_path().empty())
      continue;

    onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
    const base::DictionaryValue* policy =
        managed_network_config_handler_->FindPolicyByGuidAndProfile(
            network->guid(), network->profile_path(), &onc_source);

    if (!policy) {
      VLOG(1) << "The policy for network " << network->path() << " with GUID "
              << network->guid() << " is not available yet.";
      // Skip this network for now. Once the policy is loaded, PolicyApplied()
      // will retry.
      continue;
    }

    VLOG(2) << "Inspecting network " << network->path();
    client_cert::ClientCertConfig cert_config;
    OncToClientCertConfig(onc_source, *policy, &cert_config);

    // Skip networks that don't have a ClientCertPattern.
    if (cert_config.client_cert_type != ::onc::client_cert::kPattern)
      continue;

    networks_to_resolve.push_back(
        NetworkAndCertPattern(network->path(), cert_config));
  }

  if (networks_to_resolve.empty()) {
    VLOG(1) << "No networks to resolve.";
    // If a resolve task is running, it will notify observers when it's
    // finished.
    if (!resolve_task_running_)
      NotifyResolveRequestCompleted();
    return;
  }

  if (resolve_task_running_) {
    VLOG(1) << "A resolve task is already running. Queue this request.";
    for (const NetworkAndCertPattern& network_and_pattern :
         networks_to_resolve) {
      queued_networks_to_resolve_.insert(network_and_pattern.service_path);
    }
    return;
  }

  VLOG(2) << "Start task for resolving client cert patterns.";
  resolve_task_running_ = true;
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FindCertificateMatches,
                     net::x509_util::DupCERTCertificateList(
                         CertLoader::Get()->all_certs()),
                     net::x509_util::DupCERTCertificateList(
                         CertLoader::Get()->system_token_client_certs()),
                     networks_to_resolve, Now()),
      base::BindOnce(&ClientCertResolver::ConfigureCertificates,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientCertResolver::ResolvePendingNetworks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void ClientCertResolver::ConfigureCertificates(
    std::vector<NetworkAndMatchingCert> matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const NetworkAndMatchingCert& match : matches) {
    MatchingCertAndResolveStatus& network_status =
        networks_status_[match.service_path];
    if (network_status.resolve_status == ResolveStatus::kResolved &&
        network_status.matching_cert == match.matching_cert) {
      // The same certificate was configured in the last ConfigureCertificates
      // call, so don't do anything for this network.
      continue;
    }
    network_status.resolve_status = ResolveStatus::kResolved;
    network_status.matching_cert = match.matching_cert;
    network_properties_changed_ = true;

    NET_LOG(EVENT) << "Configuring certificate for network: "
                   << match.service_path;

    base::DictionaryValue shill_properties;
    if (match.matching_cert.has_value()) {
      const MatchingCert& matching_cert = match.matching_cert.value();
      client_cert::SetShillProperties(
          match.cert_config_type, matching_cert.key_slot_id,
          matching_cert.pkcs11_id, &shill_properties);
      if (!matching_cert.identity.empty()) {
        shill_properties.SetKey(shill::kEapIdentityProperty,
                                base::Value(matching_cert.identity));
      }
    } else {
      client_cert::SetEmptyShillProperties(match.cert_config_type,
                                           &shill_properties);
    }
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperties(
        dbus::ObjectPath(match.service_path), shill_properties,
        base::DoNothing(), base::BindRepeating(&LogError, match.service_path));
    network_state_handler_->RequestUpdateForNetwork(match.service_path);
  }
  resolve_task_running_ = false;
  if (queued_networks_to_resolve_.empty())
    NotifyResolveRequestCompleted();
  else
    ResolvePendingNetworks();
}

void ClientCertResolver::NotifyResolveRequestCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!resolve_task_running_);

  VLOG(2) << "Notify observers: " << (network_properties_changed_ ? "" : "no ")
          << "networks changed.";
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
