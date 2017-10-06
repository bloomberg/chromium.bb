// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_reporting/error_reporter.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"

namespace certificate_reporting {

namespace {

// Constants used for crypto. The corresponding private key is used by
// the SafeBrowsing client-side detection server to decrypt reports.
static const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};

static const uint32_t kServerPublicKeyVersion = 1;

static const char kHkdfLabel[] = "certificate report";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation(
        "safe_browsing_certificate_error_reporting",
        R"(
        semantics {
          sender: "Safe Browsing Extended Reporting"
          description:
            "When a user has opted in to Safe Browsing Extended Reporting, "
            "Chrome will send information about HTTPS certificate errors that "
            "the user encounters to Google. This information includes the "
            "certificate chain and the type of error encountered. The "
            "information is used to understand and mitigate common causes of "
            "spurious certificate errors."
          trigger:
            "When the user encounters an HTTPS certificate error and has opted "
            "in to Safe Browsing Extended Reporting."
          data:
            "The hostname that was requested, the certificate chain, the time "
            "of the request, information about the type of certificate error "
            "that was encountered, and whether the user was opted in to a "
            "limited set of field trials that affect certificate validation."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via the 'Automatically report "
            "details of possible security incidents to Google' setting under "
            "'Privacy'. The feature is disabled by default."
          chrome_policy {
            SafeBrowsingExtendedReportingOptInAllowed {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingOptInAllowed: false
            }
          }
        })");

}  // namespace

ErrorReporter::ErrorReporter(net::URLRequestContext* request_context,
                             const GURL& upload_url)
    : ErrorReporter(upload_url,
                    kServerPublicKey,
                    kServerPublicKeyVersion,
                    base::MakeUnique<net::ReportSender>(request_context,
                                                        kTrafficAnnotation)) {}

ErrorReporter::ErrorReporter(
    const GURL& upload_url,
    const uint8_t server_public_key[/* 32 */],
    const uint32_t server_public_key_version,
    std::unique_ptr<net::ReportSender> certificate_report_sender)
    : certificate_report_sender_(std::move(certificate_report_sender)),
      upload_url_(upload_url),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK(certificate_report_sender_);
  DCHECK(!upload_url.is_empty());
}

ErrorReporter::~ErrorReporter() {}

void ErrorReporter::SendExtendedReportingReport(
    const std::string& serialized_report,
    const base::Callback<void()>& success_callback,
    const base::Callback<void(const GURL&, int, int)>& error_callback) {
  if (upload_url_.SchemeIsCryptographic()) {
    certificate_report_sender_->Send(upload_url_, "application/octet-stream",
                                     serialized_report, success_callback,
                                     error_callback);
    return;
  }
  encrypted_messages::EncryptedMessage encrypted_report;
  // By mistake, the HKDF label here ends up with an extra null byte on
  // the end, due to using sizeof(kHkdfLabel) in the StringPiece
  // constructor instead of strlen(kHkdfLabel). This has since been changed
  // to strlen() + 1, but will need to be fixed in future to just be strlen.
  // TODO(estark): fix this...
  //  https://crbug.com/517746
  if (!encrypted_messages::EncryptSerializedMessage(
          server_public_key_, server_public_key_version_,
          base::StringPiece(kHkdfLabel, strlen(kHkdfLabel) + 1),
          serialized_report, &encrypted_report)) {
    LOG(ERROR) << "Failed to encrypt serialized report.";
    return;
  }
  std::string serialized_encrypted_report;
  encrypted_report.SerializeToString(&serialized_encrypted_report);
  certificate_report_sender_->Send(upload_url_, "application/octet-stream",
                                   serialized_encrypted_report,
                                   success_callback, error_callback);
}

}  // namespace certificate_reporting
