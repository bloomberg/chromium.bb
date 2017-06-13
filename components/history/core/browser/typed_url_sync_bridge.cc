// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_sync_bridge.h"

#include "base/big_endian.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_backend.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"

using syncer::EntityData;
using sync_pb::TypedUrlSpecifics;
using syncer::MutableDataBatch;

namespace history {

namespace {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

// There's no limit on how many visits the history DB could have for a given
// typed URL, so we limit how many we fetch from the DB to avoid crashes due to
// running out of memory (http://crbug.com/89793). This value is different
// from kMaxTypedUrlVisits, as some of the visits fetched from the DB may be
// RELOAD visits, which will be stripped.
static const int kMaxVisitsToFetch = 1000;

// Enforce oldest to newest visit order.
static bool CheckVisitOrdering(const VisitVector& visits) {
  int64_t previous_visit_time = 0;
  for (VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    if (visit != visits.begin() &&
        previous_visit_time > visit->visit_time.ToInternalValue())
      return false;

    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

std::string GetStorageKeyFromURLRow(const URLRow& row) {
  std::string storage_key(sizeof(row.id()), 0);
  base::WriteBigEndian<URLID>(&storage_key[0], row.id());
  return storage_key;
}

}  // namespace

TypedURLSyncBridge::TypedURLSyncBridge(
    HistoryBackend* history_backend,
    TypedURLSyncMetadataDatabase* sync_metadata_database,
    const ChangeProcessorFactory& change_processor_factory)
    : ModelTypeSyncBridge(change_processor_factory, syncer::TYPED_URLS),
      history_backend_(history_backend),
      sync_metadata_database_(sync_metadata_database),
      num_db_accesses_(0),
      num_db_errors_(0),
      history_backend_observer_(this) {
  DCHECK(history_backend_);
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(sync_metadata_database_);
}

TypedURLSyncBridge::~TypedURLSyncBridge() {
  // TODO(gangwu): unregister as HistoryBackendObserver, can use  ScopedObserver
  // to do it.
}

std::unique_ptr<syncer::MetadataChangeList>
TypedURLSyncBridge::CreateMetadataChangeList() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return base::MakeUnique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::TYPED_URLS);
}

base::Optional<syncer::ModelError> TypedURLSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
  return {};
}

base::Optional<syncer::ModelError> TypedURLSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
  return {};
}

void TypedURLSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::GetAllData(DataCallback callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  history::URLRows typed_urls;
  ++num_db_accesses_;
  if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
    ++num_db_errors_;
    change_processor()->ReportError(FROM_HERE,
                                    "Could not get the typed_url entries.");
    return;
  }

  auto batch = base::MakeUnique<MutableDataBatch>();
  for (history::URLRow& url : typed_urls) {
    VisitVector visits_vector;
    FixupURLAndGetVisits(&url, &visits_vector);
    batch->Put(GetStorageKeyFromURLRow(url),
               CreateEntityData(url, visits_vector));
  }
  callback.Run(std::move(batch));
}

// Must be exactly the value of GURL::spec() for backwards comparability with
// the previous (Directory + SyncableService) iteration of sync integration.
// This can be large but it is assumed that this is not held in memory at steady
// state.
std::string TypedURLSyncBridge::GetClientTag(const EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(entity_data.specifics.has_typed_url())
      << "EntityData does not have typed urls specifics.";

  return entity_data.specifics.typed_url().url();
}

// Prefer to use URLRow::id() to uniquely identify entities when coordinating
// with sync because it has a significantly low memory cost than a URL.
std::string TypedURLSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(history_backend_);
  DCHECK(entity_data.specifics.has_typed_url())
      << "EntityData does not have typed urls specifics.";

  const TypedUrlSpecifics& typed_url(entity_data.specifics.typed_url());
  URLRow existing_url;
  ++num_db_accesses_;
  bool is_existing_url =
      history_backend_->GetURL(GURL(typed_url.url()), &existing_url);

  if (!is_existing_url) {
    // The typed url did not save to local history database yet, so return URL
    // for now.
    return entity_data.specifics.typed_url().url();
  }

  return GetStorageKeyFromURLRow(existing_url);
}

void TypedURLSyncBridge::OnURLVisited(history::HistoryBackend* history_backend,
                                      ui::PageTransition transition,
                                      const history::URLRow& row,
                                      const history::RedirectList& redirects,
                                      base::Time visit_time) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::OnURLsModified(
    history::HistoryBackend* history_backend,
    const history::URLRows& changed_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::OnURLsDeleted(history::HistoryBackend* history_backend,
                                       bool all_history,
                                       bool expired,
                                       const history::URLRows& deleted_rows,
                                       const std::set<GURL>& favicon_urls) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  NOTIMPLEMENTED();
}

void TypedURLSyncBridge::Init() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  history_backend_observer_.Add(history_backend_);
  LoadMetadata();
}

int TypedURLSyncBridge::GetErrorPercentage() const {
  return num_db_accesses_ ? (100 * num_db_errors_ / num_db_accesses_) : 0;
}

bool TypedURLSyncBridge::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    TypedUrlSpecifics* typed_url) {
  DCHECK(!url.last_visit().is_null());
  DCHECK(!visits.empty());
  DCHECK_EQ(url.last_visit().ToInternalValue(),
            visits.back().visit_time.ToInternalValue());

  typed_url->set_url(url.url().spec());
  typed_url->set_title(base::UTF16ToUTF8(url.title()));
  typed_url->set_hidden(url.hidden());

  DCHECK(CheckVisitOrdering(visits));

  bool only_typed = false;
  int skip_count = 0;

  if (std::find_if(visits.begin(), visits.end(),
                   [](const history::VisitRow& visit) {
                     return ui::PageTransitionCoreTypeIs(
                         visit.transition, ui::PAGE_TRANSITION_TYPED);
                   }) == visits.end()) {
    // This URL has no TYPED visits, don't sync it
    return false;
  }

  if (visits.size() > static_cast<size_t>(kMaxTypedUrlVisits)) {
    int typed_count = 0;
    int total = 0;
    // Walk the passed-in visit vector and count the # of typed visits.
    for (VisitRow visit : visits) {
      // We ignore reload visits.
      if (PageTransitionCoreTypeIs(visit.transition,
                                   ui::PAGE_TRANSITION_RELOAD)) {
        continue;
      }
      ++total;
      if (PageTransitionCoreTypeIs(visit.transition,
                                   ui::PAGE_TRANSITION_TYPED)) {
        ++typed_count;
      }
    }

    // We should have at least one typed visit. This can sometimes happen if
    // the history DB has an inaccurate count for some reason (there's been
    // bugs in the history code in the past which has left users in the wild
    // with incorrect counts - http://crbug.com/84258).
    DCHECK(typed_count > 0);

    if (typed_count > kMaxTypedUrlVisits) {
      only_typed = true;
      skip_count = typed_count - kMaxTypedUrlVisits;
    } else if (total > kMaxTypedUrlVisits) {
      skip_count = total - kMaxTypedUrlVisits;
    }
  }

  for (const auto& visit : visits) {
    // Skip reload visits.
    if (PageTransitionCoreTypeIs(visit.transition, ui::PAGE_TRANSITION_RELOAD))
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed && !PageTransitionCoreTypeIs(visit.transition,
                                                ui::PAGE_TRANSITION_TYPED)) {
      continue;
    }

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed || !PageTransitionCoreTypeIs(visit.transition,
                                                  ui::PAGE_TRANSITION_TYPED)) {
        --skip_count;
        continue;
      }
    }
    typed_url->add_visits(visit.visit_time.ToInternalValue());
    typed_url->add_visit_transitions(visit.transition);
  }
  DCHECK_EQ(skip_count, 0);

  CHECK_GT(typed_url->visits_size(), 0);
  CHECK_LE(typed_url->visits_size(), kMaxTypedUrlVisits);
  CHECK_EQ(typed_url->visits_size(), typed_url->visit_transitions_size());

  return true;
}

void TypedURLSyncBridge::LoadMetadata() {
  if (!history_backend_ || !sync_metadata_database_) {
    change_processor()->ReportError(
        FROM_HERE, "Failed to load TypedURLSyncMetadataDatabase.");
    return;
  }

  auto batch = base::MakeUnique<syncer::MetadataBatch>();
  if (!sync_metadata_database_->GetAllSyncMetadata(batch.get())) {
    change_processor()->ReportError(
        FROM_HERE,
        "Failed reading typed url metadata from TypedURLSyncMetadataDatabase.");
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

void TypedURLSyncBridge::ClearErrorStats() {
  num_db_accesses_ = 0;
  num_db_errors_ = 0;
}

bool TypedURLSyncBridge::FixupURLAndGetVisits(URLRow* url,
                                              VisitVector* visits) {
  ++num_db_accesses_;
  if (!history_backend_->GetMostRecentVisitsForURL(url->id(), kMaxVisitsToFetch,
                                                   visits)) {
    ++num_db_errors_;
    // Couldn't load the visits for this URL due to some kind of DB error.
    // Don't bother writing this URL to the history DB (if we ignore the
    // error and continue, we might end up duplicating existing visits).
    DLOG(ERROR) << "Could not load visits for url: " << url->url();
    return false;
  }

  // Sometimes (due to a bug elsewhere in the history or sync code, or due to
  // a crash between adding a URL to the history database and updating the
  // visit DB) the visit vector for a URL can be empty. If this happens, just
  // create a new visit whose timestamp is the same as the last_visit time.
  // This is a workaround for http://crbug.com/84258.
  if (visits->empty()) {
    DVLOG(1) << "Found empty visits for URL: " << url->url();
    if (url->last_visit().is_null()) {
      // If modified URL is bookmarked, history backend treats it as modified
      // even if all its visits are deleted. Return false to stop further
      // processing because sync expects valid visit time for modified entry.
      return false;
    }

    VisitRow visit(url->id(), url->last_visit(), 0, ui::PAGE_TRANSITION_TYPED,
                   0);
    visits->push_back(visit);
  }

  // GetMostRecentVisitsForURL() returns the data in the opposite order that
  // we need it, so reverse it.
  std::reverse(visits->begin(), visits->end());

  // Sometimes, the last_visit field in the URL doesn't match the timestamp of
  // the last visit in our visit array (they come from different tables, so
  // crashes/bugs can cause them to mismatch), so just set it here.
  url->set_last_visit(visits->back().visit_time);
  DCHECK(CheckVisitOrdering(*visits));

  // Removes all visits that are older than the current expiration time. Visits
  // are in ascending order now, so we can check from beginning to check how
  // many expired visits.
  size_t num_expired_visits = 0;
  for (auto& visit : *visits) {
    base::Time time = visit.visit_time;
    if (history_backend_->IsExpiredVisitTime(time)) {
      ++num_expired_visits;
    } else {
      break;
    }
  }
  if (num_expired_visits != 0) {
    if (num_expired_visits == visits->size()) {
      DVLOG(1) << "All visits are expired for url: " << url->url();
      visits->clear();
      return false;
    }
    visits->erase(visits->begin(), visits->begin() + num_expired_visits);
  }
  DCHECK(CheckVisitOrdering(*visits));

  return true;
}

std::unique_ptr<EntityData> TypedURLSyncBridge::CreateEntityData(
    const URLRow& row,
    const VisitVector& visits) {
  auto entity_data = base::MakeUnique<EntityData>();
  TypedUrlSpecifics* specifics = entity_data->specifics.mutable_typed_url();

  if (!WriteToTypedUrlSpecifics(row, visits, specifics)) {
    // Cannot write to specifics, ex. no TYPED visits.
    return base::MakeUnique<EntityData>();
  }
  entity_data->non_unique_name = row.url().spec();

  return entity_data;
}

}  // namespace history
