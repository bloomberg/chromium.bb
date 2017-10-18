// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_assistant.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/ssl_error_assistant.pb.h"
#include "chrome/common/features.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

std::unique_ptr<std::unordered_set<std::string>> LoadCaptivePortalCertHashes(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto hashes = base::MakeUnique<std::unordered_set<std::string>>();
  for (const chrome_browser_ssl::CaptivePortalCert& cert :
       proto.captive_portal_cert()) {
    hashes.get()->insert(cert.sha256_hash());
  }
  return hashes;
}

std::unique_ptr<std::vector<MITMSoftwareType>> LoadMITMSoftwareList(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto mitm_software_list = base::MakeUnique<std::vector<MITMSoftwareType>>();

  for (const chrome_browser_ssl::MITMSoftware& proto_entry :
       proto.mitm_software()) {
    // The |name| field and at least one of the |issuer_common_name_regex| and
    // |issuer_organization_regex| fields must be set.
    DCHECK(!proto_entry.name().empty());
    DCHECK(!(proto_entry.issuer_common_name_regex().empty() &&
             proto_entry.issuer_organization_regex().empty()));
    if (proto_entry.name().empty() ||
        (proto_entry.issuer_common_name_regex().empty() &&
         proto_entry.issuer_organization_regex().empty())) {
      continue;
    }

    mitm_software_list.get()->push_back(MITMSoftwareType(
        proto_entry.name(), proto_entry.issuer_common_name_regex(),
        proto_entry.issuer_organization_regex()));
  }
  return mitm_software_list;
}

// Reads the SSL error assistant configuration from the resource bundle.
std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
ReadErrorAssistantProtoFromResourceBundle() {
  auto proto = base::MakeUnique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(proto);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  base::StringPiece data =
      bundle.GetRawDataResource(IDR_SSL_ERROR_ASSISTANT_PB);
  google::protobuf::io::ArrayInputStream stream(data.data(), data.size());
  return proto->ParseFromZeroCopyStream(&stream) ? std::move(proto) : nullptr;
}

bool RegexMatchesAny(const std::vector<std::string>& organization_names,
                     const std::string& pattern) {
  const re2::RE2 regex(pattern);
  for (const std::string& organization_name : organization_names) {
    if (re2::RE2::FullMatch(organization_name, regex)) {
      return true;
    }
  }
  return false;
}

}  // namespace

MITMSoftwareType::MITMSoftwareType(const std::string& name,
                                   const std::string& issuer_common_name_regex,
                                   const std::string& issuer_organization_regex)
    : name(name),
      issuer_common_name_regex(issuer_common_name_regex),
      issuer_organization_regex(issuer_organization_regex) {}

SSLErrorAssistant::SSLErrorAssistant() {}

SSLErrorAssistant::~SSLErrorAssistant() {}

bool SSLErrorAssistant::IsKnownCaptivePortalCertificate(
    const net::SSLInfo& ssl_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!captive_portal_spki_hashes_) {
    error_assistant_proto_ = ReadErrorAssistantProtoFromResourceBundle();
    CHECK(error_assistant_proto_);
    captive_portal_spki_hashes_ =
        LoadCaptivePortalCertHashes(*error_assistant_proto_);
  }

  for (const net::HashValue& hash_value : ssl_info.public_key_hashes) {
    if (hash_value.tag != net::HASH_VALUE_SHA256) {
      continue;
    }
    if (captive_portal_spki_hashes_->find(hash_value.ToString()) !=
        captive_portal_spki_hashes_->end()) {
      return true;
    }
  }
  return false;
}

const std::string SSLErrorAssistant::MatchKnownMITMSoftware(
    const scoped_refptr<net::X509Certificate>& cert) {
  // Ignore if the certificate doesn't have an issuer common name or an
  // organization name.
  if (cert->issuer().common_name.empty() &&
      cert->issuer().organization_names.size() == 0) {
    return std::string();
  }

  // Load MITM software data from the SSL error assistant proto.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!mitm_software_list_) {
    if (!error_assistant_proto_)
      error_assistant_proto_ = ReadErrorAssistantProtoFromResourceBundle();
    DCHECK(error_assistant_proto_);
    mitm_software_list_ = LoadMITMSoftwareList(*error_assistant_proto_);
  }

  for (const MITMSoftwareType& mitm_software : *mitm_software_list_) {
    // At least one of the common name or organization name fields should be
    // populated on the MITM software list entry.
    DCHECK(!(mitm_software.issuer_common_name_regex.empty() &&
             mitm_software.issuer_organization_regex.empty()));
    if (mitm_software.issuer_common_name_regex.empty() &&
        mitm_software.issuer_organization_regex.empty()) {
      continue;
    }

    // If both |issuer_common_name_regex| and |issuer_organization_regex| are
    // set, the certificate should match both regexes.
    if (!mitm_software.issuer_common_name_regex.empty() &&
        !mitm_software.issuer_organization_regex.empty()) {
      if (re2::RE2::FullMatch(
              cert->issuer().common_name,
              re2::RE2(mitm_software.issuer_common_name_regex)) &&
          RegexMatchesAny(cert->issuer().organization_names,
                          mitm_software.issuer_organization_regex)) {
        return mitm_software.name;
      }

      // If only |issuer_organization_regex| is set, the certificate's issuer
      // organization name should match.
    } else if (!mitm_software.issuer_organization_regex.empty()) {
      if (RegexMatchesAny(cert->issuer().organization_names,
                          mitm_software.issuer_organization_regex)) {
        return mitm_software.name;
      }

      // If only |issuer_common_name_regex| is set, the certificate's issuer
      // common name should match.
    } else if (!mitm_software.issuer_common_name_regex.empty()) {
      if (re2::RE2::FullMatch(
              cert->issuer().common_name,
              re2::RE2(mitm_software.issuer_common_name_regex))) {
        return mitm_software.name;
      }
    }
  }
  return std::string();
}

void SSLErrorAssistant::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(proto);
  if (!error_assistant_proto_) {
    // If the user hasn't seen an SSL error and a component update is available,
    // the local resource bundle won't have been read and error_assistant_proto_
    // will be null. It's possible that the local resource bundle has a higher
    // version_id than the component updater component, so load the local
    // resource bundle once to compare versions.
    // TODO(meacer): Ideally, ReadErrorAssistantProtoFromResourceBundle should
    // only be called once and not on the UI thread. Move the call to the
    // component updater component.
    error_assistant_proto_ = ReadErrorAssistantProtoFromResourceBundle();
  }

  // Ignore versions that are not new. INT_MAX is used by tests, so always allow
  // it.
  if (error_assistant_proto_ && proto->version_id() != INT_MAX &&
      proto->version_id() <= error_assistant_proto_->version_id()) {
    return;
  }

  error_assistant_proto_ = std::move(proto);

  mitm_software_list_ = LoadMITMSoftwareList(*error_assistant_proto_);

  captive_portal_spki_hashes_ =
      LoadCaptivePortalCertHashes(*error_assistant_proto_);
}

void SSLErrorAssistant::ResetForTesting() {
  error_assistant_proto_.reset();
  mitm_software_list_.reset();
  captive_portal_spki_hashes_.reset();
}

int SSLErrorAssistant::GetErrorAssistantProtoVersionIdForTesting() const {
  return error_assistant_proto_->version_id();
}
