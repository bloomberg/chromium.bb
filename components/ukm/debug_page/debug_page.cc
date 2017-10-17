// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug_page/debug_page.h"

#include <inttypes.h>

#include "base/memory/ref_counted_memory.h"
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

DebugPage::DebugPage(ServiceGetter service_getter)
    : service_getter_(service_getter) {}

DebugPage::~DebugPage() {}

std::string DebugPage::GetSource() const {
  return "ukm";
}

std::string DebugPage::GetMimeType(const std::string& path) const {
  return "text/html";
}

void DebugPage::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string data;
  data.append(R"""(<!DOCTYPE html>
  <html>
    <head>
      <meta http-equiv="Content-Security-Policy"
            content="object-src 'none'; script-src 'none'">
      <title>UKM Debug</title>
    </head>
    <body>
      <h1>UKM Debug page</h1>
  )""");
  UkmService* ukm_service = service_getter_.Run();
  if (ukm_service) {
    data.append(
        // 'id' attribute set so tests can extract this element.
        base::StringPrintf("<p>IsEnabled:<span id='state'>%s</span></p>",
                           ukm_service->recording_enabled_ ? "True" : "False"));
    data.append(base::StringPrintf("<p>ClientId:%" PRIu64 "</p>",
                                   ukm_service->client_id_));
    data.append(
        base::StringPrintf("<p>SessionId:%d</p>", ukm_service->session_id_));

    auto decode_map = ::ukm::builders::CreateDecodeMap();
    std::map<SourceId, SourceData> source_data;
    for (const auto& kv : ukm_service->sources_) {
      source_data[kv.first].source = kv.second.get();
    }

    for (const auto& v : ukm_service->entries_) {
      source_data[v.get()->source_id].entries.push_back(v.get());
    }

    data.append("<h2>Sources</h2>");
    for (const auto& kv : source_data) {
      const auto* src = kv.second.source;
      if (src) {
        data.append(base::StringPrintf("<h3>Id:%" PRId64 " Url:%s</h3>",
                                       src->id(), src->url().spec().c_str()));
      } else {
        data.append(base::StringPrintf("<h3>Id:%" PRId64 "</h3>", kv.first));
      }
      for (auto* entry : kv.second.entries) {
        data.append(
            base::StringPrintf("<h4>Entry:%s</h4>",
                               GetName(decode_map, entry->event_hash).c_str()));
        for (const auto& metric : entry->metrics) {
          data.append(base::StringPrintf(
              "<h5>Metric:%s Value:%" PRId64 "</h5>",
              GetName(decode_map, metric->metric_hash).c_str(), metric->value));
        }
      }
    }
  }

  data.append(R"""(
    </body>
  </html>
  )""");

  callback.Run(base::RefCountedString::TakeString(&data));
}

bool DebugPage::AllowCaching() const {
  return false;
}

}  // namespace debug
}  // namespace ukm
