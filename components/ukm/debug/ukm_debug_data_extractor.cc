// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug/ukm_debug_data_extractor.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_source.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "url/gurl.h"

namespace ukm {
namespace debug {

namespace {

struct SourceData {
  UkmSource* source;
  std::vector<mojom::UkmEntry*> entries;
};

std::string GetName(ukm::builders::DecodeMap& decode_map, uint64_t hash) {
  if (decode_map.count(hash))
    return decode_map[hash];
  return base::StringPrintf("Unknown %" PRIu64, hash);
}

}  // namespace

UkmDebugDataExtractor::UkmDebugDataExtractor() = default;

UkmDebugDataExtractor::~UkmDebugDataExtractor() = default;

// static
std::string UkmDebugDataExtractor::GetHTMLData(UkmService* ukm_service) {
  std::string output;
  output.append(R"""(<!DOCTYPE html>
  <html>
    <head>
      <meta http-equiv="Content-Security-Policy"
            content="object-src 'none'; script-src 'none'">
      <title>UKM Debug</title>
    </head>
    <body>
      <h1>UKM Debug page</h1>
  )""");

  if (ukm_service) {
    output.append(
        // 'id' attribute set so tests can extract this element.
        base::StringPrintf("<p>IsEnabled:<span id='state'>%s</span></p>",
                           ukm_service->recording_enabled_ ? "True" : "False"));
    output.append(base::StringPrintf("<p>ClientId:<span id='clientid'>%" PRIu64
                                     "</span></p>",
                                     ukm_service->client_id_));
    output.append(
        base::StringPrintf("<p>SessionId:%d</p>", ukm_service->session_id_));

    auto decode_map = ::ukm::builders::CreateDecodeMap();
    std::map<SourceId, SourceData> source_data;
    for (const auto& kv : ukm_service->sources_) {
      source_data[kv.first].source = kv.second.get();
    }

    for (const auto& v : ukm_service->entries_) {
      source_data[v.get()->source_id].entries.push_back(v.get());
    }

    output.append("<h2>Sources</h2>");
    for (const auto& kv : source_data) {
      const auto* src = kv.second.source;
      if (src) {
        output.append(base::StringPrintf("<h3>Id:%" PRId64 " Url:%s</h3>",
                                         src->id(), src->url().spec().c_str()));
      } else {
        output.append(base::StringPrintf("<h3>Id:%" PRId64 "</h3>", kv.first));
      }
      for (auto* entry : kv.second.entries) {
        output.append(
            base::StringPrintf("<h4>Entry:%s</h4>",
                               GetName(decode_map, entry->event_hash).c_str()));
        for (const auto& metric : entry->metrics) {
          output.append(base::StringPrintf(
              "<h5>Metric:%s Value:%" PRId64 "</h5>",
              GetName(decode_map, metric->metric_hash).c_str(), metric->value));
        }
      }
    }
  }

  output.append(R"""(
    </body>
  </html>
  )""");

  return output;
}

}  // namespace debug
}  // namespace ukm
