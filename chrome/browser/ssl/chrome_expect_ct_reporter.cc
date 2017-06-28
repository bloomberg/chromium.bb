// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_expect_ct_reporter.h"

#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "net/cert/ct_serialization.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/report_sender.h"

namespace {

std::string TimeToISO8601(const base::Time& t) {
  base::Time::Exploded exploded;
  t.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
}

std::unique_ptr<base::ListValue> GetPEMEncodedChainAsList(
    const net::X509Certificate* cert_chain) {
  if (!cert_chain)
    return base::MakeUnique<base::ListValue>();

  std::unique_ptr<base::ListValue> result(new base::ListValue());
  std::vector<std::string> pem_encoded_chain;
  cert_chain->GetPEMEncodedChain(&pem_encoded_chain);
  for (const std::string& cert : pem_encoded_chain)
    result->Append(base::MakeUnique<base::Value>(cert));

  return result;
}

std::string SCTOriginToString(
    net::ct::SignedCertificateTimestamp::Origin origin) {
  switch (origin) {
    case net::ct::SignedCertificateTimestamp::SCT_EMBEDDED:
      return "embedded";
    case net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION:
      return "tls-extension";
    case net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE:
      return "ocsp";
    case net::ct::SignedCertificateTimestamp::SCT_ORIGIN_MAX:
      NOTREACHED();
  }
  return "";
}

void AddSCT(const net::SignedCertificateTimestampAndStatus& sct,
            base::ListValue* list) {
  std::unique_ptr<base::DictionaryValue> list_item(new base::DictionaryValue());
  // Chrome implements RFC6962, not 6962-bis, so the reports contain v1 SCTs.
  list_item->SetInteger("version", 1);
  std::string status;
  switch (sct.status) {
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
      status = "unknown";
      break;
    case net::ct::SCT_STATUS_INVALID_SIGNATURE:
    case net::ct::SCT_STATUS_INVALID_TIMESTAMP:
      status = "invalid";
      break;
    case net::ct::SCT_STATUS_OK:
      status = "valid";
      break;
    case net::ct::SCT_STATUS_NONE:
      NOTREACHED();
  }
  list_item->SetString("status", status);
  list_item->SetString("source", SCTOriginToString(sct.sct->origin));
  std::string serialized_sct;
  net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
  std::string encoded_serialized_sct;
  base::Base64Encode(serialized_sct, &encoded_serialized_sct);
  list_item->SetString("serialized_sct", encoded_serialized_sct);
  list->Append(std::move(list_item));
}

// Records an UMA histogram of the net errors when Expect CT reports
// fail to send.
void RecordUMAOnFailure(const GURL& report_uri,
                        int net_error,
                        int http_response_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("SSL.ExpectCTReportFailure2", -net_error);
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("chrome_expect_ct_reporter", R"(
        semantics {
          sender: "Expect-CT reporting for Certificate Transparency reporting"
          description:
            "Websites can opt in to have Chrome send reports to them when "
            "Chrome observes connections to that website that do not meet "
            "Chrome's Certificate Transparency policy. Websites can use this "
            "feature to discover misconfigurations that prevent them from "
            "complying with Chrome's Certificate Transparency policy."
          trigger: "Website request."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and the Signed Certificate Timestamps "
            "observed on the connection."
          destination: OTHER
        }
        policy {
          cookies_allowed: false
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, this is a feature that websites can opt into and "
            "thus there is no Chrome-wide policy to disable it."
        })");

}  // namespace

ChromeExpectCTReporter::ChromeExpectCTReporter(
    net::URLRequestContext* request_context)
    : report_sender_(
          new net::ReportSender(request_context, kTrafficAnnotation)) {}

ChromeExpectCTReporter::~ChromeExpectCTReporter() {}

void ChromeExpectCTReporter::OnExpectCTFailed(
    const net::HostPortPair& host_port_pair,
    const GURL& report_uri,
    base::Time expiration,
    const net::X509Certificate* validated_certificate_chain,
    const net::X509Certificate* served_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (report_uri.is_empty())
    return;

  if (!base::FeatureList::IsEnabled(features::kExpectCTReporting))
    return;

  base::DictionaryValue outer_report;
  base::DictionaryValue* report = outer_report.SetDictionary(
      "expect-ct-report", base::MakeUnique<base::DictionaryValue>());
  report->SetString("hostname", host_port_pair.host());
  report->SetInteger("port", host_port_pair.port());
  report->SetString("date-time", TimeToISO8601(base::Time::Now()));
  report->SetString("effective-expiration-date", TimeToISO8601(expiration));
  report->Set("served-certificate-chain",
              GetPEMEncodedChainAsList(served_certificate_chain));
  report->Set("validated-certificate-chain",
              GetPEMEncodedChainAsList(validated_certificate_chain));

  std::unique_ptr<base::ListValue> scts(new base::ListValue());
  for (const auto& sct_and_status : signed_certificate_timestamps) {
    AddSCT(sct_and_status, scts.get());
  }
  report->Set("scts", std::move(scts));

  std::string serialized_report;
  if (!base::JSONWriter::Write(outer_report, &serialized_report)) {
    LOG(ERROR) << "Failed to serialize Expect CT report";
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("SSL.ExpectCTReportSendingAttempt", true);

  report_sender_->Send(report_uri, "application/json; charset=utf-8",
                       serialized_report, base::Callback<void()>(),
                       base::Bind(RecordUMAOnFailure));
}
