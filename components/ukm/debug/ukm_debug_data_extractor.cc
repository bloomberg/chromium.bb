// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug/ukm_debug_data_extractor.h"

#include <inttypes.h>

#include <map>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
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

std::string GetName(const ukm::builders::EntryDecoder& decoder, uint64_t hash) {
  const auto it = decoder.metric_map.find(hash);
  if (it == decoder.metric_map.end())
    return base::StringPrintf("Unknown %" PRIu64, hash);
  return it->second;
}

base::Value ConvertEntryToValue(const ukm::builders::DecodeMap& decode_map,
                                const mojom::UkmEntry& entry) {
  base::DictionaryValue entry_value;

  const auto it = decode_map.find(entry.event_hash);
  if (it == decode_map.end()) {
    entry_value.SetKey("name",
                       base::Value(static_cast<double>(entry.event_hash)));
  } else {
    entry_value.SetKey("name", base::Value(it->second.name));

    base::ListValue metrics_list;
    auto* metrics_list_storage = &metrics_list.GetList();
    for (const auto& metric : entry.metrics) {
      base::DictionaryValue metric_value;
      metric_value.SetKey("name",
                          base::Value(GetName(it->second, metric.first)));
      metric_value.SetKey("value",
                          base::Value(static_cast<double>(metric.second)));
      metrics_list_storage->push_back(std::move(metric_value));
    }
    entry_value.SetKey("metrics", std::move(metrics_list));
  }
  return std::move(entry_value);
}

}  // namespace

UkmDebugDataExtractor::UkmDebugDataExtractor() = default;

UkmDebugDataExtractor::~UkmDebugDataExtractor() = default;

// static
base::Value UkmDebugDataExtractor::GetStructuredData(
    const UkmService* ukm_service) {
  if (!ukm_service)
    return {};

  base::DictionaryValue ukm_data;
  ukm_data.SetKey("state", base::Value(ukm_service->recording_enabled_));
  ukm_data.SetKey("client_id",
                  base::Value(static_cast<double>(ukm_service->client_id_)));
  ukm_data.SetKey("session_id",
                  base::Value(static_cast<double>(ukm_service->session_id_)));

  std::map<SourceId, SourceData> source_data;
  for (const auto& kv : ukm_service->recordings_.sources) {
    source_data[kv.first].source = kv.second.get();
  }

  for (const auto& v : ukm_service->recordings_.entries) {
    source_data[v->source_id].entries.push_back(v.get());
  }

  base::ListValue sources_list;
  auto* source_list_storage = &sources_list.GetList();
  for (const auto& kv : source_data) {
    const auto* src = kv.second.source;

    base::DictionaryValue source_value;
    if (src) {
      source_value.SetKey("id", base::Value(static_cast<double>(src->id())));
      source_value.SetKey("url", base::Value(src->url().spec()));
    } else {
      source_value.SetKey("id", base::Value(static_cast<double>(kv.first)));
    }

    base::ListValue entries_list;
    auto* entries_list_storage = &entries_list.GetList();
    for (auto* entry : kv.second.entries) {
      entries_list_storage->push_back(
          ConvertEntryToValue(ukm_service->decode_map_, *entry));
    }

    source_value.SetKey("entries", std::move(entries_list));

    source_list_storage->push_back(std::move(source_value));
  }
  ukm_data.SetKey("sources", std::move(sources_list));
  return std::move(ukm_data);
}

// static
std::string UkmDebugDataExtractor::GetHTMLData(const UkmService* ukm_service) {
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

    const auto& decode_map = ukm_service->decode_map_;
    std::map<SourceId, SourceData> source_data;
    for (const auto& kv : ukm_service->recordings_.sources) {
      source_data[kv.first].source = kv.second.get();
    }

    for (const auto& v : ukm_service->recordings_.entries) {
      source_data[v->source_id].entries.push_back(v.get());
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
        const auto it = decode_map.find(entry->event_hash);
        if (it == decode_map.end()) {
          output.append(base::StringPrintf(
              "<h4>Entry: Unknown %" PRIu64 "</h4>", entry->event_hash));
          continue;
        }
        output.append(base::StringPrintf("<h4>Entry:%s</h4>", it->second.name));
        for (const auto& metric : entry->metrics) {
          output.append(base::StringPrintf(
              "<h5>Metric:%s Value:%" PRId64 "</h5>",
              GetName(it->second, metric.first).c_str(), metric.second));
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
