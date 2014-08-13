// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_ssl_host_state_delegate.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace {

// Switch value that specifies that certificate decisions should be forgotten at
// the end of the current session.
const int64 kForgetAtSessionEndSwitchValue = -1;

// Experiment information
const char kRememberCertificateErrorDecisionsFieldTrialName[] =
    "RememberCertificateErrorDecisions";
const char kRememberCertificateErrorDecisionsFieldTrialDefaultGroup[] =
    "Default";
const char kRememberCertificateErrorDecisionsFieldTrialLengthParam[] = "length";

// Keys for the per-site error + certificate finger to judgement content
// settings map.
const char kSSLCertDecisionCertErrorMapKey[] = "cert_exceptions_map";
const char kSSLCertDecisionExpirationTimeKey[] = "decision_expiration_time";
const char kSSLCertDecisionVersionKey[] = "version";

const int kDefaultSSLCertDecisionVersion = 1;

// All SSL decisions are per host (and are shared arcoss schemes), so this
// canonicalizes all hosts into a secure scheme GURL to use with content
// settings. The returned GURL will be the passed in host with an empty path and
// https:// as the scheme.
GURL GetSecureGURLForHost(const std::string& host) {
  std::string url = "https://" + host;
  return GURL(url);
}

// This is a helper function that returns the length of time before a
// certificate decision expires based on the command line flags. Returns a
// non-negative value in seconds or a value of -1  indicating that decisions
// should not be remembered after the current session has ended (but should be
// remembered indefinitely as long as the session does not end), which is the
// "old" style of certificate decision memory. Uses the experimental group
// unless overridden by a command line flag.
int64 GetExpirationDelta() {
  // Check command line flags first to give them priority, then check
  // experimental groups.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRememberCertErrorDecisions)) {
    std::string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRememberCertErrorDecisions);
    int64 expiration_delta;
    if (!base::StringToInt64(base::StringPiece(switch_value),
                             &expiration_delta) ||
        expiration_delta < kForgetAtSessionEndSwitchValue) {
      LOG(ERROR) << "Failed to parse the certificate error decision "
                 << "memory length: " << switch_value;
      return kForgetAtSessionEndSwitchValue;
    }

    return expiration_delta;
  }

  // If the user is in the field trial, set the expiration to the length
  // associated with that experimental group.  The default group cannot have
  // parameters associated with it, so it needs to be handled explictly.
  std::string group_name = base::FieldTrialList::FindFullName(
      kRememberCertificateErrorDecisionsFieldTrialName);
  if (!group_name.empty() &&
      group_name.compare(
          kRememberCertificateErrorDecisionsFieldTrialDefaultGroup) != 0) {
    int64 field_trial_param_length;
    std::string param = variations::GetVariationParamValue(
        kRememberCertificateErrorDecisionsFieldTrialName,
        kRememberCertificateErrorDecisionsFieldTrialLengthParam);
    if (!param.empty() && base::StringToInt64(base::StringPiece(param),
                                              &field_trial_param_length)) {
      return field_trial_param_length;
    }
  }

  return kForgetAtSessionEndSwitchValue;
}

std::string GetKey(net::X509Certificate* cert, net::CertStatus error) {
  // Since a security decision will be made based on the fingerprint, Chrome
  // should use the SHA-256 fingerprint for the certificate.
  net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateChainFingerprint256(
          cert->os_cert_handle(), cert->GetIntermediateCertificates());
  std::string base64_fingerprint;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(fingerprint.data),
                        sizeof(fingerprint.data)),
      &base64_fingerprint);
  return base::UintToString(error) + base64_fingerprint;
}

}  // namespace

// This helper function gets the dictionary of certificate fingerprints to
// errors of certificates that have been accepted by the user from the content
// dictionary that has been passed in. The returned pointer is owned by the the
// argument dict that is passed in.
//
// If create_entries is set to |DoNotCreateDictionaryEntries|,
// GetValidCertDecisionsDict will return NULL if there is anything invalid about
// the setting, such as an invalid version or invalid value types (in addition
// to there not be any values in the dictionary). If create_entries is set to
// |CreateDictionaryEntries|, if no dictionary is found or the decisions are
// expired, a new dictionary will be created
base::DictionaryValue* ChromeSSLHostStateDelegate::GetValidCertDecisionsDict(
    base::DictionaryValue* dict,
    CreateDictionaryEntriesDisposition create_entries) {
  // Extract the version of the certificate decision structure from the content
  // setting.
  int version;
  bool success = dict->GetInteger(kSSLCertDecisionVersionKey, &version);
  if (!success) {
    if (create_entries == DoNotCreateDictionaryEntries)
      return NULL;

    dict->SetInteger(kSSLCertDecisionVersionKey,
                     kDefaultSSLCertDecisionVersion);
    version = kDefaultSSLCertDecisionVersion;
  }

  // If the version is somehow a newer version than Chrome can handle, there's
  // really nothing to do other than fail silently and pretend it doesn't exist
  // (or is malformed).
  if (version > kDefaultSSLCertDecisionVersion) {
    LOG(ERROR) << "Failed to parse a certificate error exception that is in a "
               << "newer version format (" << version << ") than is supported ("
               << kDefaultSSLCertDecisionVersion << ")";
    return NULL;
  }

  // Extract the certificate decision's expiration time from the content
  // setting. If there is no expiration time, that means it should never expire
  // and it should reset only at session restart, so skip all of the expiration
  // checks.
  bool expired = false;
  base::Time now = clock_->Now();
  base::Time decision_expiration;
  if (dict->HasKey(kSSLCertDecisionExpirationTimeKey)) {
    std::string decision_expiration_string;
    int64 decision_expiration_int64;
    success = dict->GetString(kSSLCertDecisionExpirationTimeKey,
                              &decision_expiration_string);
    if (!base::StringToInt64(base::StringPiece(decision_expiration_string),
                             &decision_expiration_int64)) {
      LOG(ERROR) << "Failed to parse a certificate error exception that has a "
                 << "bad value for an expiration time: "
                 << decision_expiration_string;
      return NULL;
    }
    decision_expiration =
        base::Time::FromInternalValue(decision_expiration_int64);
  }

  // Check to see if the user's certificate decision has expired.
  // - Expired and |create_entries| is DoNotCreateDictionaryEntries, return
  // NULL.
  // - Expired and |create_entries| is CreateDictionaryEntries, update the
  // expiration time.
  if (should_remember_ssl_decisions_ !=
          ForgetSSLExceptionDecisionsAtSessionEnd &&
      decision_expiration.ToInternalValue() <= now.ToInternalValue()) {
    if (create_entries == DoNotCreateDictionaryEntries)
      return NULL;

    expired = true;

    base::Time expiration_time =
        now + default_ssl_cert_decision_expiration_delta_;
    // Unfortunately, JSON (and thus content settings) doesn't support int64
    // values, only doubles. Since this mildly depends on precision, it is
    // better to store the value as a string.
    dict->SetString(kSSLCertDecisionExpirationTimeKey,
                    base::Int64ToString(expiration_time.ToInternalValue()));
  }

  // Extract the map of certificate fingerprints to errors from the setting.
  base::DictionaryValue* cert_error_dict = NULL;  // Will be owned by dict
  if (expired ||
      !dict->GetDictionary(kSSLCertDecisionCertErrorMapKey, &cert_error_dict)) {
    if (create_entries == DoNotCreateDictionaryEntries)
      return NULL;

    cert_error_dict = new base::DictionaryValue();
    // dict takes ownership of cert_error_dict
    dict->Set(kSSLCertDecisionCertErrorMapKey, cert_error_dict);
  }

  return cert_error_dict;
}

// If |should_remember_ssl_decisions_| is
// ForgetSSLExceptionDecisionsAtSessionEnd, that means that all invalid
// certificate proceed decisions should be forgotten when the session ends. At
// attempt is made in the destructor to remove the entries, but in the case that
// things didn't shut down cleanly, on start, Clear is called to guarantee a
// clean state.
ChromeSSLHostStateDelegate::ChromeSSLHostStateDelegate(Profile* profile)
    : clock_(new base::DefaultClock()), profile_(profile) {
  int64 expiration_delta = GetExpirationDelta();
  if (expiration_delta == kForgetAtSessionEndSwitchValue) {
    should_remember_ssl_decisions_ = ForgetSSLExceptionDecisionsAtSessionEnd;
    expiration_delta = 0;
    Clear();
  } else {
    should_remember_ssl_decisions_ = RememberSSLExceptionDecisionsForDelta;
  }
  default_ssl_cert_decision_expiration_delta_ =
      base::TimeDelta::FromSeconds(expiration_delta);
}

ChromeSSLHostStateDelegate::~ChromeSSLHostStateDelegate() {
  if (should_remember_ssl_decisions_ == ForgetSSLExceptionDecisionsAtSessionEnd)
    Clear();
}

void ChromeSSLHostStateDelegate::DenyCert(const std::string& host,
                                          net::X509Certificate* cert,
                                          net::CertStatus error) {
  ChangeCertPolicy(host, cert, error, net::CertPolicy::DENIED);
}

void ChromeSSLHostStateDelegate::AllowCert(const std::string& host,
                                           net::X509Certificate* cert,
                                           net::CertStatus error) {
  ChangeCertPolicy(host, cert, error, net::CertPolicy::ALLOWED);
}

void ChromeSSLHostStateDelegate::Clear() {
  profile_->GetHostContentSettingsMap()->ClearSettingsForOneType(
      CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS);
}

net::CertPolicy::Judgment ChromeSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    net::X509Certificate* cert,
    net::CertStatus error) {
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
  GURL url = GetSecureGURLForHost(host);
  scoped_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY))
    return net::CertPolicy::UNKNOWN;

  base::DictionaryValue* dict;  // Owned by value
  int policy_decision;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  base::DictionaryValue* cert_error_dict;  // Owned by value
  cert_error_dict =
      GetValidCertDecisionsDict(dict, DoNotCreateDictionaryEntries);
  if (!cert_error_dict)
    return net::CertPolicy::UNKNOWN;

  success = cert_error_dict->GetIntegerWithoutPathExpansion(GetKey(cert, error),
                                                            &policy_decision);

  // If a policy decision was successfully retrieved and it's a valid value of
  // ALLOWED or DENIED, return the valid value. Otherwise, return UNKNOWN.
  if (success && policy_decision == net::CertPolicy::Judgment::ALLOWED)
    return net::CertPolicy::Judgment::ALLOWED;
  else if (success && policy_decision == net::CertPolicy::Judgment::DENIED)
    return net::CertPolicy::Judgment::DENIED;

  return net::CertPolicy::Judgment::UNKNOWN;
}

void ChromeSSLHostStateDelegate::RevokeAllowAndDenyPreferences(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();

  map->SetWebsiteSetting(pattern,
                         pattern,
                         CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                         std::string(),
                         NULL);
}

bool ChromeSSLHostStateDelegate::HasAllowedOrDeniedCert(
    const std::string& host) {
  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();

  scoped_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY))
    return false;

  base::DictionaryValue* dict;  // Owned by value
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    int policy_decision;  // Owned by dict
    success = it.value().GetAsInteger(&policy_decision);
    if (success && (static_cast<net::CertPolicy::Judgment>(policy_decision) !=
                    net::CertPolicy::UNKNOWN))
      return true;
  }

  return false;
}

void ChromeSSLHostStateDelegate::SetClock(scoped_ptr<base::Clock> clock) {
  clock_.reset(clock.release());
}

void ChromeSSLHostStateDelegate::ChangeCertPolicy(
    const std::string& host,
    net::X509Certificate* cert,
    net::CertStatus error,
    net::CertPolicy::Judgment judgment) {
  GURL url = GetSecureGURLForHost(host);
  const ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(url);
  HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
  scoped_ptr<base::Value> value(map->GetWebsiteSetting(
      url, url, CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, std::string(), NULL));

  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY))
    value.reset(new base::DictionaryValue());

  base::DictionaryValue* dict;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);

  base::DictionaryValue* cert_dict =
      GetValidCertDecisionsDict(dict, CreateDictionaryEntries);
  // If a a valid certificate dictionary cannot be extracted from the content
  // setting, that means it's in an unknown format. Unfortunately, there's
  // nothing to be done in that case, so a silent fail is the only option.
  if (!cert_dict)
    return;

  dict->SetIntegerWithoutPathExpansion(kSSLCertDecisionVersionKey,
                                       kDefaultSSLCertDecisionVersion);
  cert_dict->SetIntegerWithoutPathExpansion(GetKey(cert, error), judgment);

  // The map takes ownership of the value, so it is released in the call to
  // SetWebsiteSetting.
  map->SetWebsiteSetting(pattern,
                         pattern,
                         CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                         std::string(),
                         value.release());
}
