// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_sync_bridge.h"

#include "base/big_endian.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_backend.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "net/base/url_util.h"

using sync_pb::TypedUrlSpecifics;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;
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

bool HasTypedUrl(const VisitVector& visits) {
  auto typed_url_visit = std::find_if(
      visits.begin(), visits.end(), [](const history::VisitRow& visit) {
        return ui::PageTransitionCoreTypeIs(visit.transition,
                                            ui::PAGE_TRANSITION_TYPED);
      });
  return typed_url_visit != visits.end();
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
}

std::unique_ptr<MetadataChangeList>
TypedURLSyncBridge::CreateMetadataChangeList() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  return base::MakeUnique<syncer::SyncMetadataStoreChangeList>(
      sync_metadata_database_, syncer::TYPED_URLS);
}

base::Optional<ModelError> TypedURLSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // Create a mapping of all local data by URLID. These will be narrowed down
  // by CreateOrUpdateUrl() to include only the entries different from sync
  // server data.
  TypedURLMap new_db_urls;

  // Get all the visits and map the URLRows by URL.
  URLVisitVectorMap local_visit_vectors;

  if (!GetValidURLsAndVisits(&local_visit_vectors, &new_db_urls)) {
    return ModelError(
        FROM_HERE, "Could not get the typed_url entries from HistoryBackend.");
  }

  // New sync data organized for different write operations to history backend.
  history::URLRows new_synced_urls;
  history::URLRows updated_synced_urls;
  TypedURLVisitVector new_synced_visits;

  // Iterate through entity_data and check for all the urls that
  // sync already knows about. CreateOrUpdateUrl() will remove urls that
  // are the same as the synced ones from |new_db_urls|.
  for (const EntityChange& entity_change : entity_data) {
    DCHECK(entity_change.data().specifics.has_typed_url());
    const TypedUrlSpecifics& specifics =
        entity_change.data().specifics.typed_url();
    if (ShouldIgnoreUrl(GURL(specifics.url())))
      continue;

    // Ignore old sync urls that don't have any transition data stored with
    // them, or transition data that does not match the visit data (will be
    // deleted below).
    if (specifics.visit_transitions_size() == 0 ||
        specifics.visit_transitions_size() != specifics.visits_size()) {
      // Generate a debug assertion to help track down http://crbug.com/91473,
      // even though we gracefully handle this case by overwriting this node.
      DCHECK_EQ(specifics.visits_size(), specifics.visit_transitions_size());
      DVLOG(1) << "Ignoring obsolete sync url with no visit transition info.";

      continue;
    }
    UpdateUrlFromServer(specifics, &new_db_urls, &local_visit_vectors,
                        &new_synced_urls, &new_synced_visits,
                        &updated_synced_urls);
  }

  for (const auto& kv : new_db_urls) {
    if (!HasTypedUrl(local_visit_vectors[kv.first])) {
      // This URL has no TYPED visits, don't sync it
      continue;
    }
    std::string storage_key = GetStorageKeyFromURLRow(kv.second);
    change_processor()->Put(
        storage_key, CreateEntityData(kv.second, local_visit_vectors[kv.first]),
        metadata_change_list.get());
  }

  base::Optional<ModelError> error = WriteToHistoryBackend(
      &new_synced_urls, &updated_synced_urls, NULL, &new_synced_visits, NULL);

  if (error)
    return error;

  for (const EntityChange& entity_change : entity_data) {
    DCHECK(entity_change.data().specifics.has_typed_url());
    std::string storage_key =
        GetStorageKeyInternal(entity_change.data().specifics.typed_url().url());
    if (storage_key.empty()) {
      // ignore entity change
    } else {
      change_processor()->UpdateStorageKey(entity_change.data(), storage_key,
                                           metadata_change_list.get());
    }
  }

  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlMergeAndStartSyncingErrors",
                           GetErrorPercentage());
  ClearErrorStats();

  return static_cast<syncer::SyncMetadataStoreChangeList*>(
             metadata_change_list.get())
      ->TakeError();
}

base::Optional<ModelError> TypedURLSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
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
  NOTREACHED() << "TypedURLSyncBridge do not support GetStorageKey.";
  return std::string();
}

bool TypedURLSyncBridge::SupportsGetStorageKey() const {
  return false;
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

// static
TypedURLSyncBridge::MergeResult TypedURLSyncBridge::MergeUrls(
    const TypedUrlSpecifics& sync_url,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<history::VisitInfo>* new_visits) {
  DCHECK(new_url);
  DCHECK_EQ(sync_url.url(), url.url().spec());
  DCHECK_EQ(sync_url.url(), new_url->url().spec());
  DCHECK(visits->size());
  DCHECK_GT(sync_url.visits_size(), 0);
  CHECK_EQ(sync_url.visits_size(), sync_url.visit_transitions_size());

  // Convert these values only once.
  base::string16 sync_url_title(base::UTF8ToUTF16(sync_url.title()));
  base::Time sync_url_last_visit = base::Time::FromInternalValue(
      sync_url.visits(sync_url.visits_size() - 1));

  // This is a bitfield representing what we'll need to update with the output
  // value.
  MergeResult different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((sync_url_title != url.title()) || (sync_url.hidden() != url.hidden())) {
    // Use the values from the most recent visit.
    if (sync_url_last_visit >= url.last_visit()) {
      new_url->set_title(sync_url_title);
      new_url->set_hidden(sync_url.hidden());
      different |= DIFF_LOCAL_ROW_CHANGED;
    } else {
      new_url->set_title(url.title());
      new_url->set_hidden(url.hidden());
      different |= DIFF_UPDATE_NODE;
    }
  } else {
    // No difference.
    new_url->set_title(url.title());
    new_url->set_hidden(url.hidden());
  }

  size_t sync_url_num_visits = sync_url.visits_size();
  size_t history_num_visits = visits->size();
  size_t sync_url_visit_index = 0;
  size_t history_visit_index = 0;
  base::Time earliest_history_time = (*visits)[0].visit_time;
  // Walk through the two sets of visits and figure out if any new visits were
  // added on either side.
  while (sync_url_visit_index < sync_url_num_visits ||
         history_visit_index < history_num_visits) {
    // Time objects are initialized to "earliest possible time".
    base::Time sync_url_time, history_time;
    if (sync_url_visit_index < sync_url_num_visits)
      sync_url_time =
          base::Time::FromInternalValue(sync_url.visits(sync_url_visit_index));
    if (history_visit_index < history_num_visits)
      history_time = (*visits)[history_visit_index].visit_time;
    if (sync_url_visit_index >= sync_url_num_visits ||
        (history_visit_index < history_num_visits &&
         sync_url_time > history_time)) {
      // We found a visit in the history DB that doesn't exist in the sync DB,
      // so mark the sync_url as modified so the caller will update the sync
      // node.
      different |= DIFF_UPDATE_NODE;
      ++history_visit_index;
    } else if (history_visit_index >= history_num_visits ||
               sync_url_time < history_time) {
      // Found a visit in the sync node that doesn't exist in the history DB, so
      // add it to our list of new visits and set the appropriate flag so the
      // caller will update the history DB.
      // If the sync_url visit is older than any existing visit in the history
      // DB, don't re-add it - this keeps us from resurrecting visits that were
      // aged out locally.
      //
      // TODO(sync): This extra check should be unnecessary now that filtering
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing history visits should still be synced, so this
      // check should be removed.
      if (sync_url_time > earliest_history_time) {
        different |= DIFF_LOCAL_VISITS_ADDED;
        new_visits->push_back(history::VisitInfo(
            sync_url_time, ui::PageTransitionFromInt(sync_url.visit_transitions(
                               sync_url_visit_index))));
      }
      // This visit is added to visits below.
      ++sync_url_visit_index;
    } else {
      // Same (already synced) entry found in both DBs - no need to do anything.
      ++sync_url_visit_index;
      ++history_visit_index;
    }
  }

  DCHECK(CheckVisitOrdering(*visits));
  if (different & DIFF_LOCAL_VISITS_ADDED) {
    // If the server does not have the same visits as the local db, then the
    // new visits from the server need to be added to the vector containing
    // local visits. These visits will be passed to the server.
    // Insert new visits into the appropriate place in the visits vector.
    history::VisitVector::iterator visit_ix = visits->begin();
    for (std::vector<history::VisitInfo>::iterator new_visit =
             new_visits->begin();
         new_visit != new_visits->end(); ++new_visit) {
      while (visit_ix != visits->end() &&
             new_visit->first > visit_ix->visit_time) {
        ++visit_ix;
      }
      visit_ix =
          visits->insert(visit_ix, history::VisitRow(url.id(), new_visit->first,
                                                     0, new_visit->second, 0));
      ++visit_ix;
    }
  }
  DCHECK(CheckVisitOrdering(*visits));

  new_url->set_last_visit(visits->back().visit_time);
  return different;
}

// static
void TypedURLSyncBridge::UpdateURLRowFromTypedUrlSpecifics(
    const TypedUrlSpecifics& typed_url,
    history::URLRow* new_url) {
  DCHECK_GT(typed_url.visits_size(), 0);
  CHECK_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  if (!new_url->url().is_valid()) {
    new_url->set_url(GURL(typed_url.url()));
  }
  new_url->set_title(base::UTF8ToUTF16(typed_url.title()));
  new_url->set_hidden(typed_url.hidden());
  // Only provide the initial value for the last_visit field - after that, let
  // the history code update the last_visit field on its own.
  if (new_url->last_visit().is_null()) {
    new_url->set_last_visit(base::Time::FromInternalValue(
        typed_url.visits(typed_url.visits_size() - 1)));
  }
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

void TypedURLSyncBridge::UpdateUrlFromServer(
    const sync_pb::TypedUrlSpecifics& server_typed_url,
    TypedURLMap* local_typed_urls,
    URLVisitVectorMap* local_visit_vectors,
    history::URLRows* new_synced_urls,
    TypedURLVisitVector* new_synced_visits,
    history::URLRows* updated_synced_urls) {
  DCHECK(server_typed_url.visits_size() != 0);
  DCHECK_EQ(server_typed_url.visits_size(),
            server_typed_url.visit_transitions_size());

  // Ignore empty urls.
  if (server_typed_url.url().empty()) {
    DVLOG(1) << "Ignoring empty URL in sync DB";
    return;
  }
  // Now, get rid of the expired visits. If there are no un-expired visits
  // left, ignore this url - any local data should just replace it.
  TypedUrlSpecifics sync_url = FilterExpiredVisits(server_typed_url);
  if (sync_url.visits_size() == 0) {
    DVLOG(1) << "Ignoring expired URL in sync DB: " << sync_url.url();
    return;
  }

  // Check if local db already has the url from sync.
  TypedURLMap::iterator it = local_typed_urls->find(GURL(sync_url.url()));
  if (it == local_typed_urls->end()) {
    // There are no matching typed urls from the local db, check for untyped
    history::URLRow untyped_url(GURL(sync_url.url()));

    // The URL may still exist in the local db if it is an untyped url.
    // An untyped url will transition to a typed url after receiving visits
    // from sync, and sync should receive any visits already existing locally
    // for the url, so the full list of visits is consistent.
    bool is_existing_url =
        history_backend_->GetURL(untyped_url.url(), &untyped_url);
    if (is_existing_url) {
      // Add a new entry to |local_typed_urls|, and set the iterator to it.
      history::VisitVector untyped_visits;
      if (!FixupURLAndGetVisits(&untyped_url, &untyped_visits)) {
        return;
      }
      (*local_visit_vectors)[untyped_url.url()] = untyped_visits;

      // Store row info that will be used to update sync's visits.
      (*local_typed_urls)[untyped_url.url()] = untyped_url;

      // Set iterator |it| to point to this entry.
      it = local_typed_urls->find(untyped_url.url());
      DCHECK(it != local_typed_urls->end());
      // Continue with merge below.
    } else {
      // The url is new to the local history DB.
      // Create new db entry for url.
      history::URLRow new_url(GURL(sync_url.url()));
      UpdateURLRowFromTypedUrlSpecifics(sync_url, &new_url);
      new_synced_urls->push_back(new_url);

      // Add entries for url visits.
      std::vector<history::VisitInfo> added_visits;
      size_t visit_count = sync_url.visits_size();

      for (size_t index = 0; index < visit_count; ++index) {
        base::Time visit_time =
            base::Time::FromInternalValue(sync_url.visits(index));
        ui::PageTransition transition =
            ui::PageTransitionFromInt(sync_url.visit_transitions(index));
        added_visits.push_back(history::VisitInfo(visit_time, transition));
      }
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<history::VisitInfo>>(new_url.url(),
                                                           added_visits));
      return;
    }
  }

  // Same URL exists in sync data and in history data - compare the
  // entries to see if there's any difference.
  history::VisitVector& visits = (*local_visit_vectors)[it->first];
  std::vector<history::VisitInfo> added_visits;

  // Empty URLs should be filtered out by ShouldIgnoreUrl() previously.
  DCHECK(!it->second.url().spec().empty());

  // Initialize fields in |new_url| to the same values as the fields in
  // the existing URLRow in the history DB. This is needed because we
  // overwrite the existing value in WriteToHistoryBackend(), but some of
  // the values in that structure are not synced (like typed_count).
  history::URLRow new_url(it->second);

  MergeResult difference =
      MergeUrls(sync_url, it->second, &visits, &new_url, &added_visits);

  if (difference != DIFF_NONE) {
    it->second = new_url;
    if (difference & DIFF_UPDATE_NODE) {
      // We don't want to resurrect old visits that have been aged out by
      // other clients, so remove all visits that are older than the
      // earliest existing visit in the sync node.
      //
      // TODO(sync): This logic should be unnecessary now that filtering of
      // expired visits is performed separately. Non-expired visits older than
      // the earliest existing sync visits should still be synced, so this
      // logic should be removed.
      if (sync_url.visits_size() > 0) {
        base::Time earliest_visit =
            base::Time::FromInternalValue(sync_url.visits(0));
        for (history::VisitVector::iterator i = visits.begin();
             i != visits.end() && i->visit_time < earliest_visit;) {
          i = visits.erase(i);
        }
        // Should never be possible to delete all the items, since the
        // visit vector contains newer local visits it will keep and/or the
        // visits in typed_url.visits newer than older local visits.
        DCHECK(visits.size() > 0);
      }
      DCHECK_EQ(new_url.last_visit().ToInternalValue(),
                visits.back().visit_time.ToInternalValue());
    }
    if (difference & DIFF_LOCAL_ROW_CHANGED) {
      // Add entry to updated_synced_urls to update the local db.
      DCHECK_EQ(it->second.id(), new_url.id());
      updated_synced_urls->push_back(new_url);
    }
    if (difference & DIFF_LOCAL_VISITS_ADDED) {
      // Add entry with new visits to new_synced_visits to update the local db.
      new_synced_visits->push_back(
          std::pair<GURL, std::vector<history::VisitInfo>>(it->first,
                                                           added_visits));
    }
  } else {
    // No difference in urls, erase from map
    local_typed_urls->erase(it);
  }
}

base::Optional<ModelError> TypedURLSyncBridge::WriteToHistoryBackend(
    const history::URLRows* new_urls,
    const history::URLRows* updated_urls,
    const std::vector<GURL>* deleted_urls,
    const TypedURLVisitVector* new_visits,
    const history::VisitVector* deleted_visits) {
  if (deleted_urls && !deleted_urls->empty())
    history_backend_->DeleteURLs(*deleted_urls);

  if (new_urls) {
    history_backend_->AddPagesWithDetails(*new_urls, history::SOURCE_SYNCED);
  }

  if (updated_urls) {
    ++num_db_accesses_;
    // This is an existing entry in the URL database. We don't verify the
    // visit_count or typed_count values here, because either one (or both)
    // could be zero in the case of bookmarks, or in the case of a URL
    // transitioning from non-typed to typed as a result of this sync.
    // In the field we sometimes run into errors on specific URLs. It's OK
    // to just continue on (we can try writing again on the next model
    // association).
    size_t num_successful_updates = history_backend_->UpdateURLs(*updated_urls);
    num_db_errors_ += updated_urls->size() - num_successful_updates;
  }

  if (new_visits) {
    for (const auto& visits : *new_visits) {
      // If there are no visits to add, just skip this.
      if (visits.second.empty())
        continue;
      ++num_db_accesses_;
      if (!history_backend_->AddVisits(visits.first, visits.second,
                                       history::SOURCE_SYNCED)) {
        ++num_db_errors_;
        return ModelError(FROM_HERE, "Could not add visits to HistoryBackend.");
      }
    }
  }

  if (deleted_visits) {
    ++num_db_accesses_;
    if (!history_backend_->RemoveVisits(*deleted_visits)) {
      ++num_db_errors_;
      return ModelError(FROM_HERE,
                        "Could not remove visits from HistoryBackend.");
      // This is bad news, since it means we may end up resurrecting history
      // entries on the next reload. It's unavoidable so we'll just keep on
      // syncing.
    }
  }

  return {};
}

TypedUrlSpecifics TypedURLSyncBridge::FilterExpiredVisits(
    const TypedUrlSpecifics& source) {
  // Make a copy of the source, then regenerate the visits.
  TypedUrlSpecifics specifics(source);
  specifics.clear_visits();
  specifics.clear_visit_transitions();
  for (int i = 0; i < source.visits_size(); ++i) {
    base::Time time = base::Time::FromInternalValue(source.visits(i));
    if (!history_backend_->IsExpiredVisitTime(time)) {
      specifics.add_visits(source.visits(i));
      specifics.add_visit_transitions(source.visit_transitions(i));
    }
  }
  DCHECK(specifics.visits_size() == specifics.visit_transitions_size());
  return specifics;
}

bool TypedURLSyncBridge::ShouldIgnoreUrl(const GURL& url) {
  // Ignore empty URLs. Not sure how this can happen (maybe import from other
  // busted browsers, or misuse of the history API, or just plain bugs) but we
  // can't deal with them.
  if (url.spec().empty())
    return true;

  // Ignore local file URLs.
  if (url.SchemeIsFile())
    return true;

  // Ignore localhost URLs.
  if (net::IsLocalhost(url.host_piece()))
    return true;

  // Ignore username and password, sonce history backend will remove user name
  // and password in URLDatabase::GURLToDatabaseURL and send username/password
  // removed url to sync later.
  if (url.has_username() || url.has_password())
    return true;

  return false;
}

bool TypedURLSyncBridge::ShouldIgnoreVisits(
    const history::VisitVector& visits) {
  // We ignore URLs that were imported, but have never been visited by
  // chromium.
  static const int kFirstImportedSource = history::SOURCE_FIREFOX_IMPORTED;
  history::VisitSourceMap map;
  if (!history_backend_->GetVisitsSource(visits, &map))
    return false;  // If we can't read the visit, assume it's not imported.

  // Walk the list of visits and look for a non-imported item.
  for (const auto& visit : visits) {
    if (map.count(visit.visit_id) == 0 ||
        map[visit.visit_id] < kFirstImportedSource) {
      return false;
    }
  }
  // We only saw imported visits, so tell the caller to ignore them.
  return true;
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

bool TypedURLSyncBridge::GetValidURLsAndVisits(URLVisitVectorMap* url_to_visit,
                                               TypedURLMap* url_to_urlrow) {
  DCHECK(url_to_visit);
  DCHECK(url_to_urlrow);

  history::URLRows local_typed_urls;
  ++num_db_accesses_;
  if (!history_backend_->GetAllTypedURLs(&local_typed_urls)) {
    ++num_db_errors_;
    return false;
  }
  for (history::URLRow& url : local_typed_urls) {
    DCHECK_EQ(0U, url_to_visit->count(url.url()));
    if (!FixupURLAndGetVisits(&url, &((*url_to_visit)[url.url()])) ||
        ShouldIgnoreUrl(url.url()) ||
        ShouldIgnoreVisits((*url_to_visit)[url.url()])) {
      // Ignore this URL if we couldn't load the visits or if there's some
      // other problem with it (it was empty, or imported and never visited).
    } else {
      // Add url to url_to_urlrow.
      (*url_to_urlrow)[url.url()] = url;
    }
  }
  return true;
}

std::string TypedURLSyncBridge::GetStorageKeyInternal(const std::string& url) {
  DCHECK(history_backend_);

  URLRow existing_url;
  ++num_db_accesses_;
  bool is_existing_url = history_backend_->GetURL(GURL(url), &existing_url);

  if (!is_existing_url) {
    // The typed url did not save to local history database, so return empty
    // string.
    return std::string();
  }

  return GetStorageKeyFromURLRow(existing_url);
}

}  // namespace history
