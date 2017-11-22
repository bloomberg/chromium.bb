// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "components/download/internal/model.h"
#include "components/download/internal/scheduler/device_status.h"
#include "components/download/public/clients.h"
#include "components/download/public/download_metadata.h"

namespace download {

struct DownloadMetaData;
struct Entry;

namespace util {

// Helper method to return the number of Entry objects in |entries| are
// associated with |client| and do not have a state of Entry::State::COMPLETE.
uint32_t GetNumberOfLiveEntriesForClient(DownloadClient client,
                                         const std::vector<Entry*>& entries);

// Effectively runs a map reduce to turn a list of Entry objects into a map of
// [Client Id] -> List of entries meta data.
// Any Entry in |entries| that does not have a matching DownloadClient in
// |clients| will be put in the DownloadClient::INVALID bucket.
// Any Entry in |entries| with an Entry::State that is in |ignored_states| will
// not be included.
std::map<DownloadClient, std::vector<DownloadMetaData>>
MapEntriesToMetadataForClients(const std::set<DownloadClient>& clients,
                               const std::vector<Entry*>& entries);

// Gets the least strict scheduling criteria from |entries|, the criteria is
// used to schedule platform background tasks.
Criteria GetSchedulingCriteria(const Model::EntryList& entries,
                               int optimal_battery_percentage);

// Returns if |lhs| entry is a better candidate to be the next download than
// |rhs| based on their priority and cancel time.
bool EntryBetterThan(const Entry& lhs, const Entry& rhs);

// Builds a download meta data based on |entry|.
DownloadMetaData BuildDownloadMetaData(Entry* entry);

}  // namespace util
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_ENTRY_UTILS_H_
