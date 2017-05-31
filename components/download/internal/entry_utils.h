// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "components/download/public/clients.h"

namespace download {

struct Entry;

namespace util {

// Helper method to return the number of Entry objects in |entries| are
// associated with |client|.
uint32_t GetNumberOfEntriesForClient(DownloadClient client,
                                     const std::vector<Entry*>& entries);

// Effectively runs a map reduce to turn a list of Entry objects into a map of
// [Client Id] -> List of entries.  Any Entry in |entries| that does not have a
// matching DownloadClient in |clients| will be put in the
// DownloadClient::INVALID bucket.
std::map<DownloadClient, std::vector<std::string>> MapEntriesToClients(
    const std::set<DownloadClient>& clients,
    const std::vector<Entry*>& entries);

}  // namespace util
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_
