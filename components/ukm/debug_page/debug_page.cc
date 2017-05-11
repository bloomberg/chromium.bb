// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug_page/debug_page.h"

#include <inttypes.h>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/stringprintf.h"
#include "components/ukm/ukm_entry.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_source.h"
#include "url/gurl.h"

namespace ukm {
namespace debug {

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
        base::StringPrintf("<p>IsEnabled:%s</p>",
                           ukm_service->recording_enabled_ ? "True" : "False"));
    data.append(base::StringPrintf("<p>ClientId:%" PRIu64 "</p>",
                                   ukm_service->client_id_));
    data.append(
        base::StringPrintf("<p>SessionId:%d</p>", ukm_service->session_id_));

    data.append("<h2>Sources</h2>");
    for (const auto& kv : ukm_service->sources_) {
      const auto* src = kv.second.get();
      data.append(base::StringPrintf("<p>Id:%d Url:%s</p>", src->id(),
                                     src->url().spec().c_str()));
    }

    data.append("<h2>Entries</h2>");
    for (const auto& v : ukm_service->entries_) {
      const auto* entry = v.get();
      data.append(base::StringPrintf("<h3>Id:%d Hash:%" PRIu64 "</h3>",
                                     entry->source_id(), entry->event_hash()));
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
