// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/cast_cert_validator_internal.h"

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <time.h>

#include <vector>

#include "cast/common/certificate/types.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr static int32_t kMinRsaModulusLengthBits = 2048;

// Stores intermediate state while attempting to find a valid certificate chain
// from a set of trusted certificates to a target certificate.  Together, a
// sequence of these forms a certificate chain to be verified as well as a stack
// that can be unwound for searching more potential paths.
struct CertPathStep {
  X509* cert;

  // The next index that can be checked in |trust_store| if the choice |cert| on
  // the path needs to be reverted.
  uint32_t trust_store_index;

  // The next index that can be checked in |intermediate_certs| if the choice
  // |cert| on the path needs to be reverted.
  uint32_t intermediate_cert_index;
};

// These values are bit positions from RFC 5280 4.2.1.3 and will be passed to
// ASN1_BIT_STRING_get_bit.
enum KeyUsageBits {
  kDigitalSignature = 0,
  kKeyCertSign = 5,
};

bool CertInPath(X509_NAME* name,
                const std::vector<CertPathStep>& steps,
                uint32_t start,
                uint32_t stop) {
  for (uint32_t i = start; i < stop; ++i) {
    if (X509_NAME_cmp(name, X509_get_subject_name(steps[i].cert)) == 0) {
      return true;
    }
  }
  return false;
}

// Parse the data in |time| at |index| as a two-digit ascii number.
uint8_t ParseAsn1TimeDoubleDigit(ASN1_GENERALIZEDTIME* time, int index) {
  return (time->data[index] - '0') * 10 + (time->data[index + 1] - '0');
}

Error::Code VerifyCertTime(X509* cert, const DateTime& time) {
  DateTime not_before;
  DateTime not_after;
  if (!GetCertValidTimeRange(cert, &not_before, &not_after)) {
    return Error::Code::kErrCertsVerifyGeneric;
  }
  if ((time < not_before) || (not_after < time)) {
    return Error::Code::kErrCertsDateInvalid;
  }
  return Error::Code::kNone;
}

bool VerifyPublicKeyLength(EVP_PKEY* public_key) {
  return EVP_PKEY_bits(public_key) >= kMinRsaModulusLengthBits;
}

bssl::UniquePtr<ASN1_BIT_STRING> GetKeyUsage(X509* cert) {
  int pos = X509_get_ext_by_NID(cert, NID_key_usage, -1);
  if (pos == -1) {
    return nullptr;
  }
  X509_EXTENSION* key_usage = X509_get_ext(cert, pos);
  const uint8_t* value = key_usage->value->data;
  ASN1_BIT_STRING* key_usage_bit_string = nullptr;
  if (!d2i_ASN1_BIT_STRING(&key_usage_bit_string, &value,
                           key_usage->value->length)) {
    return nullptr;
  }
  return bssl::UniquePtr<ASN1_BIT_STRING>{key_usage_bit_string};
}

Error::Code VerifyCertificateChain(const std::vector<CertPathStep>& path,
                                   uint32_t step_index,
                                   const DateTime& time) {
  // Default max path length is the number of intermediate certificates.
  int max_pathlen = path.size() - 2;

  std::vector<NAME_CONSTRAINTS*> path_name_constraints;
  Error::Code error = Error::Code::kNone;
  uint32_t i = step_index;
  for (; i < path.size() - 1; ++i) {
    X509* subject = path[i + 1].cert;
    X509* issuer = path[i].cert;
    bool is_root = (i == step_index);
    if (!is_root) {
      if ((error = VerifyCertTime(issuer, time)) != Error::Code::kNone) {
        return error;
      }
      if (X509_NAME_cmp(X509_get_subject_name(issuer),
                        X509_get_issuer_name(issuer)) != 0) {
        if (max_pathlen == 0) {
          return Error::Code::kErrCertsPathlen;
        }
        --max_pathlen;
      } else {
        issuer->ex_flags |= EXFLAG_SI;
      }
    } else {
      issuer->ex_flags |= EXFLAG_SI;
    }

    bssl::UniquePtr<ASN1_BIT_STRING> key_usage = GetKeyUsage(issuer);
    if (key_usage) {
      const int bit =
          ASN1_BIT_STRING_get_bit(key_usage.get(), KeyUsageBits::kKeyCertSign);
      if (bit == 0) {
        return Error::Code::kErrCertsVerifyGeneric;
      }
    }

    // Check that basicConstraints is present, specifies the CA bit, and use
    // pathLenConstraint if present.
    const int basic_constraints_index =
        X509_get_ext_by_NID(issuer, NID_basic_constraints, -1);
    if (basic_constraints_index == -1) {
      return Error::Code::kErrCertsVerifyGeneric;
    }
    X509_EXTENSION* const basic_constraints_extension =
        X509_get_ext(issuer, basic_constraints_index);
    bssl::UniquePtr<BASIC_CONSTRAINTS> basic_constraints{
        reinterpret_cast<BASIC_CONSTRAINTS*>(
            X509V3_EXT_d2i(basic_constraints_extension))};

    if (!basic_constraints || !basic_constraints->ca) {
      return Error::Code::kErrCertsVerifyGeneric;
    }

    if (basic_constraints->pathlen) {
      if (basic_constraints->pathlen->length != 1) {
        return Error::Code::kErrCertsVerifyGeneric;
      } else {
        const int pathlen = *basic_constraints->pathlen->data;
        if (pathlen < 0) {
          return Error::Code::kErrCertsVerifyGeneric;
        }
        if (pathlen < max_pathlen) {
          max_pathlen = pathlen;
        }
      }
    }

    if (X509_ALGOR_cmp(issuer->sig_alg, issuer->cert_info->signature) != 0) {
      return Error::Code::kErrCertsVerifyGeneric;
    }

    bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(issuer)};
    if (!VerifyPublicKeyLength(public_key.get())) {
      return Error::Code::kErrCertsVerifyGeneric;
    }

    // NOTE: (!self-issued || target) -> verify name constraints.  Target case
    // is after the loop.
    const bool is_self_issued = issuer->ex_flags & EXFLAG_SI;
    if (!is_self_issued) {
      for (NAME_CONSTRAINTS* name_constraints : path_name_constraints) {
        if (NAME_CONSTRAINTS_check(subject, name_constraints) != X509_V_OK) {
          return Error::Code::kErrCertsVerifyGeneric;
        }
      }
    }

    if (issuer->nc) {
      path_name_constraints.push_back(issuer->nc);
    } else {
      const int index = X509_get_ext_by_NID(issuer, NID_name_constraints, -1);
      if (index != -1) {
        X509_EXTENSION* ext = X509_get_ext(issuer, index);
        auto* nc = reinterpret_cast<NAME_CONSTRAINTS*>(X509V3_EXT_d2i(ext));
        if (nc) {
          issuer->nc = nc;
          path_name_constraints.push_back(nc);
        } else {
          return Error::Code::kErrCertsVerifyGeneric;
        }
      }
    }

    // Check that any policy mappings present are _not_ the anyPolicy OID.  Even
    // though we don't otherwise handle policies, this is required by RFC 5280
    // 6.1.4(a).
    const int policy_mappings_index =
        X509_get_ext_by_NID(issuer, NID_policy_mappings, -1);
    if (policy_mappings_index != -1) {
      X509_EXTENSION* policy_mappings_extension =
          X509_get_ext(issuer, policy_mappings_index);
      auto* policy_mappings = reinterpret_cast<POLICY_MAPPINGS*>(
          X509V3_EXT_d2i(policy_mappings_extension));
      const uint32_t policy_mapping_count =
          sk_POLICY_MAPPING_num(policy_mappings);
      const ASN1_OBJECT* any_policy = OBJ_nid2obj(NID_any_policy);
      for (uint32_t i = 0; i < policy_mapping_count; ++i) {
        POLICY_MAPPING* policy_mapping =
            sk_POLICY_MAPPING_value(policy_mappings, i);
        const bool either_matches =
            ((OBJ_cmp(policy_mapping->issuerDomainPolicy, any_policy) == 0) ||
             (OBJ_cmp(policy_mapping->subjectDomainPolicy, any_policy) == 0));
        if (either_matches) {
          error = Error::Code::kErrCertsVerifyGeneric;
          break;
        }
      }
      sk_POLICY_MAPPING_free(policy_mappings);
      if (error != Error::Code::kNone) {
        return error;
      }
    }

    // Check that we don't have any unhandled extensions marked as critical.
    int extension_count = X509_get_ext_count(issuer);
    for (int i = 0; i < extension_count; ++i) {
      X509_EXTENSION* extension = X509_get_ext(issuer, i);
      if (extension->critical > 0) {
        const int nid = OBJ_obj2nid(extension->object);
        if (nid != NID_name_constraints && nid != NID_basic_constraints &&
            nid != NID_key_usage) {
          return Error::Code::kErrCertsVerifyGeneric;
        }
      }
    }

    int nid = OBJ_obj2nid(subject->sig_alg->algorithm);
    const EVP_MD* digest;
    switch (nid) {
      case NID_sha1WithRSAEncryption:
        digest = EVP_sha1();
        break;
      case NID_sha256WithRSAEncryption:
        digest = EVP_sha256();
        break;
      case NID_sha384WithRSAEncryption:
        digest = EVP_sha384();
        break;
      case NID_sha512WithRSAEncryption:
        digest = EVP_sha512();
        break;
      default:
        return Error::Code::kErrCertsVerifyGeneric;
    }
    if (!VerifySignedData(
            digest, public_key.get(),
            {subject->cert_info->enc.enc,
             static_cast<uint32_t>(subject->cert_info->enc.len)},
            {subject->signature->data,
             static_cast<uint32_t>(subject->signature->length)})) {
      return Error::Code::kErrCertsVerifyGeneric;
    }
  }
  // NOTE: Other half of ((!self-issued || target) -> check name constraints).
  for (NAME_CONSTRAINTS* name_constraints : path_name_constraints) {
    if (NAME_CONSTRAINTS_check(path.back().cert, name_constraints) !=
        X509_V_OK) {
      return Error::Code::kErrCertsVerifyGeneric;
    }
  }
  return error;
}

X509* ParseX509Der(const std::string& der) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(der.data());
  return d2i_X509(nullptr, &data, der.size());
}

}  // namespace

// Parses DateTime with additional restrictions laid out by RFC 5280
// 4.1.2.5.2.
bool ParseAsn1GeneralizedTime(ASN1_GENERALIZEDTIME* time, DateTime* out) {
  static constexpr uint8_t kDaysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };

  if (time->length != 15) {
    return false;
  }
  if (time->data[14] != 'Z') {
    return false;
  }
  for (int i = 0; i < 14; ++i) {
    if (time->data[i] < '0' || time->data[i] > '9') {
      return false;
    }
  }
  out->year = ParseAsn1TimeDoubleDigit(time, 0) * 100 +
              ParseAsn1TimeDoubleDigit(time, 2);
  out->month = ParseAsn1TimeDoubleDigit(time, 4);
  out->day = ParseAsn1TimeDoubleDigit(time, 6);
  out->hour = ParseAsn1TimeDoubleDigit(time, 8);
  out->minute = ParseAsn1TimeDoubleDigit(time, 10);
  out->second = ParseAsn1TimeDoubleDigit(time, 12);
  if (out->month == 0 || out->month > 12) {
    return false;
  }
  int days_per_month = kDaysPerMonth[out->month - 1];
  if (out->month == 2) {
    if (out->year % 4 == 0 && (out->year % 100 != 0 || out->year % 400 == 0)) {
      days_per_month = 29;
    } else {
      days_per_month = 28;
    }
  }
  if (out->day == 0 || out->day > days_per_month) {
    return false;
  }
  if (out->hour > 23) {
    return false;
  }
  if (out->minute > 59) {
    return false;
  }
  // Leap seconds are allowed.
  if (out->second > 60) {
    return false;
  }
  return true;
}

bool GetCertValidTimeRange(X509* cert,
                           DateTime* not_before,
                           DateTime* not_after) {
  ASN1_GENERALIZEDTIME* not_before_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notBefore, nullptr);
  ASN1_GENERALIZEDTIME* not_after_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notAfter, nullptr);
  if (!not_before_asn1 || !not_after_asn1) {
    return false;
  }
  bool times_valid = ParseAsn1GeneralizedTime(not_before_asn1, not_before) &&
                     ParseAsn1GeneralizedTime(not_after_asn1, not_after);
  ASN1_GENERALIZEDTIME_free(not_before_asn1);
  ASN1_GENERALIZEDTIME_free(not_after_asn1);
  return times_valid;
}

bool VerifySignedData(const EVP_MD* digest,
                      EVP_PKEY* public_key,
                      const ConstDataSpan& data,
                      const ConstDataSpan& signature) {
  // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
  // |digest|.
  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr, public_key)) {
    return false;
  }
  return (EVP_DigestVerify(ctx.get(), signature.data, signature.length,
                           data.data, data.length) == 1);
}

Error FindCertificatePath(const std::vector<std::string>& der_certs,
                          const DateTime& time,
                          CertificatePathResult* result_path,
                          TrustStore* trust_store) {
  if (der_certs.empty()) {
    return Error::Code::kErrCertsMissing;
  }

  bssl::UniquePtr<X509>& target_cert = result_path->target_cert;
  std::vector<bssl::UniquePtr<X509>>& intermediate_certs =
      result_path->intermediate_certs;
  target_cert.reset(ParseX509Der(der_certs[0]));
  if (!target_cert) {
    return Error::Code::kErrCertsParse;
  }
  for (size_t i = 1; i < der_certs.size(); ++i) {
    intermediate_certs.emplace_back(ParseX509Der(der_certs[i]));
    if (!intermediate_certs.back()) {
      return Error::Code::kErrCertsParse;
    }
  }

  // Basic checks on the target certificate.
  Error::Code error = VerifyCertTime(target_cert.get(), time);
  if (error != Error::Code::kNone) {
    return error;
  }
  bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(target_cert.get())};
  if (!VerifyPublicKeyLength(public_key.get())) {
    return Error::Code::kErrCertsVerifyGeneric;
  }
  if (X509_ALGOR_cmp(target_cert.get()->sig_alg,
                     target_cert.get()->cert_info->signature) != 0) {
    return Error::Code::kErrCertsVerifyGeneric;
  }
  bssl::UniquePtr<ASN1_BIT_STRING> key_usage = GetKeyUsage(target_cert.get());
  if (!key_usage) {
    return Error::Code::kErrCertsRestrictions;
  }
  int bit =
      ASN1_BIT_STRING_get_bit(key_usage.get(), KeyUsageBits::kDigitalSignature);
  if (bit == 0) {
    return Error::Code::kErrCertsRestrictions;
  }

  X509* path_head = target_cert.get();
  std::vector<CertPathStep> path;

  // This vector isn't used as resizable, so instead we allocate the largest
  // possible single path up front.  This would be a single trusted cert, all
  // the intermediate certs used once, and the target cert.
  path.resize(1 + intermediate_certs.size() + 1);

  // Additionally, the path is slightly simpler to deal with if the list is
  // sorted from trust->target, so the path is actually built starting from the
  // end.
  uint32_t first_index = path.size() - 1;
  path[first_index].cert = path_head;

  // Index into |path| of the current frontier of path construction.
  uint32_t path_index = first_index;

  // Whether |path| has reached a certificate in |trust_store| and is ready for
  // verification.
  bool path_cert_in_trust_store = false;

  // Attempt to build a valid certificate chain from |target_cert| to a
  // certificate in |trust_store|.  This loop tries all possible paths in a
  // depth-first-search fashion.  If no valid paths are found, the error
  // returned is whatever the last error was from the last path tried.
  uint32_t trust_store_index = 0;
  uint32_t intermediate_cert_index = 0;
  Error::Code last_error = Error::Code::kNone;
  for (;;) {
    X509_NAME* target_issuer_name = X509_get_issuer_name(path_head);

    // The next issuer certificate to add to the current path.
    X509* next_issuer = nullptr;

    for (uint32_t i = trust_store_index; i < trust_store->certs.size(); ++i) {
      X509* trust_store_cert = trust_store->certs[i].get();
      X509_NAME* trust_store_cert_name =
          X509_get_subject_name(trust_store_cert);
      if (X509_NAME_cmp(trust_store_cert_name, target_issuer_name) == 0) {
        CertPathStep& next_step = path[--path_index];
        next_step.cert = trust_store_cert;
        next_step.trust_store_index = i + 1;
        next_step.intermediate_cert_index = 0;
        next_issuer = trust_store_cert;
        path_cert_in_trust_store = true;
        break;
      }
    }
    trust_store_index = 0;
    if (!next_issuer) {
      for (uint32_t i = intermediate_cert_index; i < intermediate_certs.size();
           ++i) {
        X509* intermediate_cert = intermediate_certs[i].get();
        X509_NAME* intermediate_cert_name =
            X509_get_subject_name(intermediate_cert);
        if (X509_NAME_cmp(intermediate_cert_name, target_issuer_name) == 0 &&
            !CertInPath(intermediate_cert_name, path, path_index,
                        first_index)) {
          CertPathStep& next_step = path[--path_index];
          next_step.cert = intermediate_cert;
          next_step.trust_store_index = trust_store->certs.size();
          next_step.intermediate_cert_index = i + 1;
          next_issuer = intermediate_cert;
          break;
        }
      }
    }
    intermediate_cert_index = 0;
    if (!next_issuer) {
      if (path_index == first_index) {
        // There are no more paths to try.  Ensure an error is returned.
        if (last_error == Error::Code::kNone) {
          return Error::Code::kErrCertsVerifyGeneric;
        }
        return last_error;
      } else {
        CertPathStep& last_step = path[path_index++];
        trust_store_index = last_step.trust_store_index;
        intermediate_cert_index = last_step.intermediate_cert_index;
        continue;
      }
    }

    if (path_cert_in_trust_store) {
      last_error = VerifyCertificateChain(path, path_index, time);
      if (last_error != Error::Code::kNone) {
        CertPathStep& last_step = path[path_index++];
        trust_store_index = last_step.trust_store_index;
        intermediate_cert_index = last_step.intermediate_cert_index;
        path_cert_in_trust_store = false;
      } else {
        break;
      }
    }
    path_head = next_issuer;
  }

  result_path->path.reserve(path.size() - path_index);
  for (uint32_t i = path_index; i < path.size(); ++i) {
    result_path->path.push_back(path[i].cert);
  }

  return Error::Code::kNone;
}

}  // namespace cast
}  // namespace openscreen
