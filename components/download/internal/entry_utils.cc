// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/entry_utils.h"

#include "components/download/internal/entry.h"

namespace download {
namespace util {

uint32_t GetNumberOfEntriesForClient(DownloadClient client,
                                     const std::vector<Entry*>& entries) {
  uint32_t count = 0;
  for (auto* entry : entries)
    if (entry->client == client)
      count++;

  return count;
}

std::map<DownloadClient, std::vector<std::string>> MapEntriesToClients(
    const std::set<DownloadClient>& clients,
    const std::vector<Entry*>& entries) {
  std::map<DownloadClient, std::vector<std::string>> categorized;

  for (auto* entry : entries) {
    DownloadClient client = entry->client;
    if (clients.find(client) == clients.end())
      client = DownloadClient::INVALID;

    categorized[client].push_back(entry->guid);
  }

  return categorized;
}

}  // namespace util
}  // namespace download
