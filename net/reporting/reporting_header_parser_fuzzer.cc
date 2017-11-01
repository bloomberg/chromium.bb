// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_client.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_policy.pb.h"
#include "net/reporting/reporting_test_util.h"
#include "url/gurl.h"

#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

// TODO: consider including proto definition for URL after moving that to
// testing/libfuzzer/proto and creating a separate converter.
const GURL kUrl_ = GURL("https://origin/path");

namespace net_reporting_header_parser_fuzzer {

void FuzzReportingHeaderParser(const std::string& data,
                              const net::ReportingPolicy& policy) {
  net::TestReportingContext context(policy);
  net::ReportingHeaderParser::ParseHeader(&context, kUrl_, data.c_str());
  std::vector<const net::ReportingClient*> clients;
  context.cache()->GetClients(&clients);
  if (clients.empty()) {
    return;
  }
}

void InitializeReportingPolicy(
    net::ReportingPolicy& policy,
    const net_reporting_policy_proto::ReportingPolicy& policy_data) {
  policy.max_report_count = policy_data.max_report_count();
  policy.max_client_count = policy_data.max_client_count();
  policy.delivery_interval =
      base::TimeDelta::FromMicroseconds(policy_data.delivery_interval_us());
  policy.persistence_interval =
      base::TimeDelta::FromMicroseconds(policy_data.persistence_interval_us());
  policy.persist_reports_across_restarts =
      policy_data.persist_reports_across_restarts();
  policy.persist_clients_across_restarts =
      policy_data.persist_clients_across_restarts();
  policy.garbage_collection_interval = base::TimeDelta::FromMicroseconds(
      policy_data.garbage_collection_interval_us());
  policy.max_report_age =
      base::TimeDelta::FromMicroseconds(policy_data.max_report_age_us());
  policy.max_report_attempts = policy_data.max_report_attempts();
  policy.clear_reports_on_network_changes =
      policy_data.clear_reports_on_network_changes();
  policy.clear_clients_on_network_changes =
      policy_data.clear_clients_on_network_changes();
}

DEFINE_BINARY_PROTO_FUZZER(
    const net_reporting_policy_proto::ReportingHeaderParserFuzzInput& input) {
  net::ReportingPolicy policy;
  InitializeReportingPolicy(policy, input.policy());

  json_proto::JsonProtoConverter converter;
  auto data = converter.Convert(input.headers());

  FuzzReportingHeaderParser(data, policy);
}

}  // namespace net_reporting_header_parser_fuzzer
