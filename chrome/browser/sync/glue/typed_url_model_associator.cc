// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_model_associator.h"

#include <algorithm>
#include <set>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/typed_url_specifics.pb.h"

using content::BrowserThread;

namespace browser_sync {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

// There's no limit on how many visits the history DB could have for a given
// typed URL, so we limit how many we fetch from the DB to avoid crashes due to
// running out of memory (http://crbug.com/89793). This value is different
// from kMaxTypedUrlVisits, as some of the visits fetched from the DB may be
// RELOAD visits, which will be stripped.
static const int kMaxVisitsToFetch = 1000;

static bool CheckVisitOrdering(const history::VisitVector& visits) {
  int64 previous_visit_time = 0;
  for (history::VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    if (visit != visits.begin()) {
      // We allow duplicate visits here - they shouldn't really be allowed, but
      // they still seem to show up sometimes and we haven't figured out the
      // source, so we just log an error instead of failing an assertion.
      // (http://crbug.com/91473).
      if (previous_visit_time == visit->visit_time.ToInternalValue())
        DVLOG(1) << "Duplicate visit time encountered";
      else if (previous_visit_time > visit->visit_time.ToInternalValue())
        return false;
    }

    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

TypedUrlModelAssociator::TypedUrlModelAssociator(
    ProfileSyncService* sync_service,
    history::HistoryBackend* history_backend,
    sync_driver::DataTypeErrorHandler* error_handler)
    : sync_service_(sync_service),
      history_backend_(history_backend),
      expected_loop_(base::MessageLoop::current()),
      abort_requested_(false),
      error_handler_(error_handler),
      num_db_accesses_(0),
      num_db_errors_(0) {
  DCHECK(sync_service_);
  // history_backend_ may be null for unit tests (since it's not mockable).
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
}

TypedUrlModelAssociator::~TypedUrlModelAssociator() {}


bool TypedUrlModelAssociator::FixupURLAndGetVisits(
    history::URLRow* url,
    history::VisitVector* visits) {
  ++num_db_accesses_;
  CHECK(history_backend_);
  if (!history_backend_->GetMostRecentVisitsForURL(
          url->id(), kMaxVisitsToFetch, visits)) {
    ++num_db_errors_;
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

    history::VisitRow visit(
        url->id(), url->last_visit(), 0, ui::PAGE_TRANSITION_TYPED, 0);
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
  return true;
}

bool TypedUrlModelAssociator::ShouldIgnoreUrl(const GURL& url) {
  // Ignore empty URLs. Not sure how this can happen (maybe import from other
  // busted browsers, or misuse of the history API, or just plain bugs) but we
  // can't deal with them.
  if (url.spec().empty())
    return true;

  // Ignore local file URLs.
  if (url.SchemeIsFile())
    return true;

  // Ignore localhost URLs.
  if (net::IsLocalhost(url.host()))
    return true;

  return false;
}

bool TypedUrlModelAssociator::ShouldIgnoreVisits(
    const history::VisitVector& visits) {
  // We ignore URLs that were imported, but have never been visited by
  // chromium.
  static const int kLastImportedSource = history::SOURCE_EXTENSION;
  history::VisitSourceMap map;
  if (!history_backend_->GetVisitsSource(visits, &map))
    return false;  // If we can't read the visit, assume it's not imported.

  // Walk the list of visits and look for a non-imported item.
  for (history::VisitVector::const_iterator it = visits.begin();
       it != visits.end(); ++it) {
    if (map.count(it->visit_id) == 0 ||
        map[it->visit_id] <= kLastImportedSource) {
      return false;
    }
  }
  // We only saw imported visits, so tell the caller to ignore them.
  return true;
}

syncer::SyncError TypedUrlModelAssociator::AssociateModels(
    syncer::SyncMergeResult* local_merge_result,
    syncer::SyncMergeResult* syncer_merge_result) {
  ClearErrorStats();
  syncer::SyncError error = DoAssociateModels();
  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlModelAssociationErrors",
                           GetErrorPercentage());
  ClearErrorStats();
  return error;
}

void TypedUrlModelAssociator::ClearErrorStats() {
  num_db_accesses_ = 0;
  num_db_errors_ = 0;
}

int TypedUrlModelAssociator::GetErrorPercentage() const {
  return num_db_accesses_ ? (100 * num_db_errors_ / num_db_accesses_) : 0;
}

syncer::SyncError TypedUrlModelAssociator::DoAssociateModels() {
  DVLOG(1) << "Associating TypedUrl Models";
  DCHECK(expected_loop_ == base::MessageLoop::current());

  history::URLRows typed_urls;
  ++num_db_accesses_;
  bool query_succeeded =
      history_backend_ && history_backend_->GetAllTypedURLs(&typed_urls);

  history::URLRows new_urls;
  history::URLRows updated_urls;
  TypedUrlVisitVector new_visits;
  {
    base::AutoLock au(abort_lock_);
    if (abort_requested_) {
      return syncer::SyncError(FROM_HERE,
                               syncer::SyncError::DATATYPE_ERROR,
                               "Association was aborted.",
                               model_type());
    }

    // Must lock and check first to make sure |error_handler_| is valid.
    if (!query_succeeded) {
      ++num_db_errors_;
      return error_handler_->CreateAndUploadError(
          FROM_HERE,
          "Could not get the typed_url entries.",
          model_type());
    }

    // Get all the visits.
    std::map<history::URLID, history::VisitVector> visit_vectors;
    for (history::URLRows::iterator ix = typed_urls.begin();
         ix != typed_urls.end();) {
      DCHECK_EQ(0U, visit_vectors.count(ix->id()));
      if (!FixupURLAndGetVisits(&(*ix), &(visit_vectors[ix->id()])) ||
          ShouldIgnoreUrl(ix->url()) ||
          ShouldIgnoreVisits(visit_vectors[ix->id()])) {
        // Ignore this URL if we couldn't load the visits or if there's some
        // other problem with it (it was empty, or imported and never visited).
        ix = typed_urls.erase(ix);
      } else {
        ++ix;
      }
    }

    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode typed_url_root(&trans);
    if (typed_url_root.InitTypeRoot(syncer::TYPED_URLS) !=
        syncer::BaseNode::INIT_OK) {
      return error_handler_->CreateAndUploadError(
          FROM_HERE,
          "Server did not create the top-level typed_url node. We "
          "might be running against an out-of-date server.",
          model_type());
    }

    std::set<std::string> current_urls;
    for (history::URLRows::iterator ix = typed_urls.begin();
         ix != typed_urls.end(); ++ix) {
      std::string tag = ix->url().spec();
      // Empty URLs should be filtered out by ShouldIgnoreUrl() previously.
      DCHECK(!tag.empty());
      history::VisitVector& visits = visit_vectors[ix->id()];

      syncer::ReadNode node(&trans);
      if (node.InitByClientTagLookup(syncer::TYPED_URLS, tag) ==
              syncer::BaseNode::INIT_OK) {
        // Same URL exists in sync data and in history data - compare the
        // entries to see if there's any difference.
        sync_pb::TypedUrlSpecifics typed_url(
            FilterExpiredVisits(node.GetTypedUrlSpecifics()));
        DCHECK_EQ(tag, typed_url.url());

        // Initialize fields in |new_url| to the same values as the fields in
        // the existing URLRow in the history DB. This is needed because we
        // overwrite the existing value below in WriteToHistoryBackend(), but
        // some of the values in that structure are not synced (like
        // typed_count).
        history::URLRow new_url(*ix);

        std::vector<history::VisitInfo> added_visits;
        MergeResult difference =
            MergeUrls(typed_url, *ix, &visits, &new_url, &added_visits);
        if (difference & DIFF_UPDATE_NODE) {
          syncer::WriteNode write_node(&trans);
          if (write_node.InitByClientTagLookup(syncer::TYPED_URLS, tag) !=
                  syncer::BaseNode::INIT_OK) {
            return error_handler_->CreateAndUploadError(
                FROM_HERE,
                "Failed to edit typed_url sync node.",
                model_type());
          }
          // We don't want to resurrect old visits that have been aged out by
          // other clients, so remove all visits that are older than the
          // earliest existing visit in the sync node.
          if (typed_url.visits_size() > 0) {
            base::Time earliest_visit =
                base::Time::FromInternalValue(typed_url.visits(0));
            for (history::VisitVector::iterator it = visits.begin();
                 it != visits.end() && it->visit_time < earliest_visit; ) {
              it = visits.erase(it);
            }
            // Should never be possible to delete all the items, since the
            // visit vector contains all the items in typed_url.visits.
            DCHECK(visits.size() > 0);
          }
          DCHECK_EQ(new_url.last_visit().ToInternalValue(),
                    visits.back().visit_time.ToInternalValue());
          WriteToSyncNode(new_url, visits, &write_node);
        }
        if (difference & DIFF_LOCAL_ROW_CHANGED) {
          DCHECK_EQ(ix->id(), new_url.id());
          updated_urls.push_back(new_url);
        }
        if (difference & DIFF_LOCAL_VISITS_ADDED) {
          new_visits.push_back(
              std::pair<GURL, std::vector<history::VisitInfo> >(ix->url(),
                                                                added_visits));
        }
      } else {
        // Sync has never seen this URL before.
        syncer::WriteNode node(&trans);
        syncer::WriteNode::InitUniqueByCreationResult result =
            node.InitUniqueByCreation(syncer::TYPED_URLS,
                                      typed_url_root, tag);
        if (result != syncer::WriteNode::INIT_SUCCESS) {
          return error_handler_->CreateAndUploadError(
              FROM_HERE,
              "Failed to create typed_url sync node: " + tag,
              model_type());
        }

        node.SetTitle(tag);
        WriteToSyncNode(*ix, visits, &node);
      }

      current_urls.insert(tag);
    }

    // Now walk the sync nodes and detect any URLs that exist there, but not in
    // the history DB, so we can add them to our local history DB.
    std::vector<int64> obsolete_nodes;
    int64 sync_child_id = typed_url_root.GetFirstChildId();
    while (sync_child_id != syncer::kInvalidId) {
      syncer::ReadNode sync_child_node(&trans);
      if (sync_child_node.InitByIdLookup(sync_child_id) !=
              syncer::BaseNode::INIT_OK) {
        return error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Failed to fetch child node.",
            model_type());
      }
      const sync_pb::TypedUrlSpecifics& typed_url(
          sync_child_node.GetTypedUrlSpecifics());

      sync_child_id = sync_child_node.GetSuccessorId();

      // Ignore old sync nodes that don't have any transition data stored with
      // them, or transition data that does not match the visit data (will be
      // deleted below).
      if (typed_url.visit_transitions_size() == 0 ||
          typed_url.visit_transitions_size() != typed_url.visits_size()) {
        // Generate a debug assertion to help track down http://crbug.com/91473,
        // even though we gracefully handle this case by throwing away this
        // node.
        DCHECK_EQ(typed_url.visits_size(), typed_url.visit_transitions_size());
        DVLOG(1) << "Deleting obsolete sync node with no visit "
                 << "transition info.";
        obsolete_nodes.push_back(sync_child_node.GetId());
        continue;
      }

      if (typed_url.url().empty()) {
        DVLOG(1) << "Ignoring empty URL in sync DB";
        continue;
      }

      // Now, get rid of the expired visits, and if there are no un-expired
      // visits left, just ignore this node.
      sync_pb::TypedUrlSpecifics filtered_url = FilterExpiredVisits(typed_url);
      if (filtered_url.visits_size() == 0) {
        DVLOG(1) << "Ignoring expired URL in sync DB: " << filtered_url.url();
        continue;
      }

      if (current_urls.find(filtered_url.url()) == current_urls.end()) {
        // Update the local DB from the sync DB. Since we are doing our
        // initial model association, we don't want to remove any of the
        // existing visits (pass NULL as |visits_to_remove|).
        UpdateFromSyncDB(filtered_url,
                         &new_visits,
                         NULL,
                         &updated_urls,
                         &new_urls);
      }
    }

    // If we encountered any obsolete nodes, remove them so they don't hang
    // around and confuse people looking at the sync node browser.
    if (!obsolete_nodes.empty()) {
      for (std::vector<int64>::const_iterator it = obsolete_nodes.begin();
           it != obsolete_nodes.end();
           ++it) {
        syncer::WriteNode sync_node(&trans);
        if (sync_node.InitByIdLookup(*it) != syncer::BaseNode::INIT_OK) {
          return error_handler_->CreateAndUploadError(
              FROM_HERE,
              "Failed to fetch obsolete node.",
              model_type());
        }
        sync_node.Tombstone();
      }
    }
  }

  // Since we're on the history thread, we don't have to worry about updating
  // the history database after closing the write transaction, since
  // this is the only thread that writes to the database.  We also don't have
  // to worry about the sync model getting out of sync, because changes are
  // propagated to the ChangeProcessor on this thread.
  WriteToHistoryBackend(&new_urls, &updated_urls, &new_visits, NULL);
  return syncer::SyncError();
}

void TypedUrlModelAssociator::UpdateFromSyncDB(
    const sync_pb::TypedUrlSpecifics& typed_url,
    TypedUrlVisitVector* visits_to_add,
    history::VisitVector* visits_to_remove,
    history::URLRows* updated_urls,
    history::URLRows* new_urls) {
  history::URLRow new_url(GURL(typed_url.url()));
  history::VisitVector existing_visits;
  bool existing_url = history_backend_->GetURL(new_url.url(), &new_url);
  if (existing_url) {
    // This URL already exists locally - fetch the visits so we can
    // merge them below.
    if (!FixupURLAndGetVisits(&new_url, &existing_visits)) {
      // Couldn't load the visits for this URL due to some kind of DB error.
      // Don't bother writing this URL to the history DB (if we ignore the
      // error and continue, we might end up duplicating existing visits).
      DLOG(ERROR) << "Could not load visits for url: " << new_url.url();
      return;
    }
  }
  visits_to_add->push_back(std::pair<GURL, std::vector<history::VisitInfo> >(
      new_url.url(), std::vector<history::VisitInfo>()));

  // Update the URL with information from the typed URL.
  UpdateURLRowFromTypedUrlSpecifics(typed_url, &new_url);

  // Figure out which visits we need to add.
  DiffVisits(existing_visits, typed_url, &visits_to_add->back().second,
             visits_to_remove);

  if (existing_url) {
    updated_urls->push_back(new_url);
  } else {
    new_urls->push_back(new_url);
  }
}

sync_pb::TypedUrlSpecifics TypedUrlModelAssociator::FilterExpiredVisits(
    const sync_pb::TypedUrlSpecifics& source) {
  // Make a copy of the source, then regenerate the visits.
  sync_pb::TypedUrlSpecifics specifics(source);
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

bool TypedUrlModelAssociator::DeleteAllNodes(
    syncer::WriteTransaction* trans) {
  DCHECK(expected_loop_ == base::MessageLoop::current());

  // Just walk through all our child nodes and delete them.
  syncer::ReadNode typed_url_root(trans);
  if (typed_url_root.InitTypeRoot(syncer::TYPED_URLS) !=
          syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << "Could not lookup root node";
    return false;
  }
  int64 sync_child_id = typed_url_root.GetFirstChildId();
  while (sync_child_id != syncer::kInvalidId) {
    syncer::WriteNode sync_child_node(trans);
    if (sync_child_node.InitByIdLookup(sync_child_id) !=
            syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << "Typed url node lookup failed.";
      return false;
    }
    sync_child_id = sync_child_node.GetSuccessorId();
    sync_child_node.Tombstone();
  }
  return true;
}

syncer::SyncError TypedUrlModelAssociator::DisassociateModels() {
  return syncer::SyncError();
}

void TypedUrlModelAssociator::AbortAssociation() {
  base::AutoLock lock(abort_lock_);
  abort_requested_ = true;
}

bool TypedUrlModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncer::ReadNode sync_node(&trans);
  if (sync_node.InitTypeRoot(syncer::TYPED_URLS) != syncer::BaseNode::INIT_OK) {
    LOG(ERROR) << "Server did not create the top-level typed_url node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the typed_url folder has any
  // children.
  *has_nodes = sync_node.HasChildren();
  return true;
}

void TypedUrlModelAssociator::WriteToHistoryBackend(
    const history::URLRows* new_urls,
    const history::URLRows* updated_urls,
    const TypedUrlVisitVector* new_visits,
    const history::VisitVector* deleted_visits) {
  if (new_urls) {
    history_backend_->AddPagesWithDetails(*new_urls, history::SOURCE_SYNCED);
  }
  if (updated_urls) {
    ++num_db_accesses_;
    // These are existing entries in the URL database. We don't verify the
    // visit_count or typed_count values here, because either one (or both)
    // could be zero in the case of bookmarks, or in the case of a URL
    // transitioning from non-typed to typed as a result of this sync.
    // In the field we sometimes run into errors on specific URLs. It's OK to
    // just continue, as we can try writing again on the next model association.
    size_t num_successful_updates = history_backend_->UpdateURLs(*updated_urls);
    num_db_errors_ += updated_urls->size() - num_successful_updates;
  }
  if (new_visits) {
    for (TypedUrlVisitVector::const_iterator visits = new_visits->begin();
         visits != new_visits->end(); ++visits) {
      // If there are no visits to add, just skip this.
      if (visits->second.empty())
        continue;
      ++num_db_accesses_;
      if (!history_backend_->AddVisits(visits->first, visits->second,
                                       history::SOURCE_SYNCED)) {
        ++num_db_errors_;
        DLOG(ERROR) << "Could not add visits.";
      }
    }
  }
  if (deleted_visits) {
    ++num_db_accesses_;
    if (!history_backend_->RemoveVisits(*deleted_visits)) {
      ++num_db_errors_;
      DLOG(ERROR) << "Could not remove visits.";
      // This is bad news, since it means we may end up resurrecting history
      // entries on the next reload. It's unavoidable so we'll just keep on
      // syncing.
    }
  }
}

// static
TypedUrlModelAssociator::MergeResult TypedUrlModelAssociator::MergeUrls(
    const sync_pb::TypedUrlSpecifics& node,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<history::VisitInfo>* new_visits) {
  DCHECK(new_url);
  DCHECK(!node.url().compare(url.url().spec()));
  DCHECK(!node.url().compare(new_url->url().spec()));
  DCHECK(visits->size());
  CHECK_EQ(node.visits_size(), node.visit_transitions_size());

  // If we have an old-format node (before we added the visits and
  // visit_transitions arrays to the protobuf) or else the node only contained
  // expired visits, so just overwrite it with our local history data.
  if (node.visits_size() == 0)
    return DIFF_UPDATE_NODE;

  // Convert these values only once.
  base::string16 node_title(base::UTF8ToUTF16(node.title()));
  base::Time node_last_visit = base::Time::FromInternalValue(
      node.visits(node.visits_size() - 1));

  // This is a bitfield representing what we'll need to update with the output
  // value.
  MergeResult different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((node_title.compare(url.title()) != 0) ||
      (node.hidden() != url.hidden())) {
    // Use the values from the most recent visit.
    if (node_last_visit >= url.last_visit()) {
      new_url->set_title(node_title);
      new_url->set_hidden(node.hidden());
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

  size_t node_num_visits = node.visits_size();
  size_t history_num_visits = visits->size();
  size_t node_visit_index = 0;
  size_t history_visit_index = 0;
  base::Time earliest_history_time = (*visits)[0].visit_time;
  // Walk through the two sets of visits and figure out if any new visits were
  // added on either side.
  while (node_visit_index < node_num_visits ||
         history_visit_index < history_num_visits) {
    // Time objects are initialized to "earliest possible time".
    base::Time node_time, history_time;
    if (node_visit_index < node_num_visits)
      node_time = base::Time::FromInternalValue(node.visits(node_visit_index));
    if (history_visit_index < history_num_visits)
      history_time = (*visits)[history_visit_index].visit_time;
    if (node_visit_index >= node_num_visits ||
        (history_visit_index < history_num_visits &&
         node_time > history_time)) {
      // We found a visit in the history DB that doesn't exist in the sync DB,
      // so mark the node as modified so the caller will update the sync node.
      different |= DIFF_UPDATE_NODE;
      ++history_visit_index;
    } else if (history_visit_index >= history_num_visits ||
               node_time < history_time) {
      // Found a visit in the sync node that doesn't exist in the history DB, so
      // add it to our list of new visits and set the appropriate flag so the
      // caller will update the history DB.
      // If the node visit is older than any existing visit in the history DB,
      // don't re-add it - this keeps us from resurrecting visits that were
      // aged out locally.
      if (node_time > earliest_history_time) {
        different |= DIFF_LOCAL_VISITS_ADDED;
        new_visits->push_back(history::VisitInfo(
            node_time,
            ui::PageTransitionFromInt(
                node.visit_transitions(node_visit_index))));
      }
      // This visit is added to visits below.
      ++node_visit_index;
    } else {
      // Same (already synced) entry found in both DBs - no need to do anything.
      ++node_visit_index;
      ++history_visit_index;
    }
  }

  DCHECK(CheckVisitOrdering(*visits));
  if (different & DIFF_LOCAL_VISITS_ADDED) {
    // Insert new visits into the apropriate place in the visits vector.
    history::VisitVector::iterator visit_ix = visits->begin();
    for (std::vector<history::VisitInfo>::iterator new_visit =
             new_visits->begin();
         new_visit != new_visits->end(); ++new_visit) {
      while (visit_ix != visits->end() &&
             new_visit->first > visit_ix->visit_time) {
        ++visit_ix;
      }
      visit_ix = visits->insert(visit_ix,
                                history::VisitRow(url.id(), new_visit->first,
                                                  0, new_visit->second, 0));
      ++visit_ix;
    }
  }
  DCHECK(CheckVisitOrdering(*visits));

  new_url->set_last_visit(visits->back().visit_time);
  return different;
}

// static
void TypedUrlModelAssociator::WriteToSyncNode(
    const history::URLRow& url,
    const history::VisitVector& visits,
    syncer::WriteNode* node) {
  sync_pb::TypedUrlSpecifics typed_url;
  WriteToTypedUrlSpecifics(url, visits, &typed_url);
  node->SetTypedUrlSpecifics(typed_url);
}

void TypedUrlModelAssociator::WriteToTypedUrlSpecifics(
    const history::URLRow& url,
    const history::VisitVector& visits,
    sync_pb::TypedUrlSpecifics* typed_url) {

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

  if (visits.size() > static_cast<size_t>(kMaxTypedUrlVisits)) {
    int typed_count = 0;
    int total = 0;
    // Walk the passed-in visit vector and count the # of typed visits.
    for (history::VisitVector::const_iterator visit = visits.begin();
         visit != visits.end(); ++visit) {
      ui::PageTransition transition =
          ui::PageTransitionStripQualifier(visit->transition);
      // We ignore reload visits.
      if (transition == ui::PAGE_TRANSITION_RELOAD)
        continue;
      ++total;
      if (transition == ui::PAGE_TRANSITION_TYPED)
        ++typed_count;
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


  for (history::VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    ui::PageTransition transition =
        ui::PageTransitionStripQualifier(visit->transition);
    // Skip reload visits.
    if (transition == ui::PAGE_TRANSITION_RELOAD)
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed && transition != ui::PAGE_TRANSITION_TYPED)
      continue;

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed || transition != ui::PAGE_TRANSITION_TYPED) {
        --skip_count;
        continue;
      }
    }
    typed_url->add_visits(visit->visit_time.ToInternalValue());
    typed_url->add_visit_transitions(visit->transition);
  }
  DCHECK_EQ(skip_count, 0);

  if (typed_url->visits_size() == 0) {
    // If we get here, it's because we don't actually have any TYPED visits
    // even though the visit's typed_count > 0 (corrupted typed_count). So
    // let's go ahead and add a RELOAD visit at the most recent visit since
    // it's not legal to have an empty visit array (yet another workaround
    // for http://crbug.com/84258).
    typed_url->add_visits(url.last_visit().ToInternalValue());
    typed_url->add_visit_transitions(ui::PAGE_TRANSITION_RELOAD);
  }
  CHECK_GT(typed_url->visits_size(), 0);
  CHECK_LE(typed_url->visits_size(), kMaxTypedUrlVisits);
  CHECK_EQ(typed_url->visits_size(), typed_url->visit_transitions_size());
}

// static
void TypedUrlModelAssociator::DiffVisits(
    const history::VisitVector& old_visits,
    const sync_pb::TypedUrlSpecifics& new_url,
    std::vector<history::VisitInfo>* new_visits,
    history::VisitVector* removed_visits) {
  DCHECK(new_visits);
  size_t old_visit_count = old_visits.size();
  size_t new_visit_count = new_url.visits_size();
  size_t old_index = 0;
  size_t new_index = 0;
  while (old_index < old_visit_count && new_index < new_visit_count) {
    base::Time new_visit_time =
        base::Time::FromInternalValue(new_url.visits(new_index));
    if (old_visits[old_index].visit_time < new_visit_time) {
      if (new_index > 0 && removed_visits) {
        // If there are visits missing from the start of the node, that
        // means that they were probably clipped off due to our code that
        // limits the size of the sync nodes - don't delete them from our
        // local history.
        removed_visits->push_back(old_visits[old_index]);
      }
      ++old_index;
    } else if (old_visits[old_index].visit_time > new_visit_time) {
      new_visits->push_back(history::VisitInfo(
          new_visit_time,
          ui::PageTransitionFromInt(
              new_url.visit_transitions(new_index))));
      ++new_index;
    } else {
      ++old_index;
      ++new_index;
    }
  }

  if (removed_visits) {
    for ( ; old_index < old_visit_count; ++old_index) {
      removed_visits->push_back(old_visits[old_index]);
    }
  }

  for ( ; new_index < new_visit_count; ++new_index) {
    new_visits->push_back(history::VisitInfo(
        base::Time::FromInternalValue(new_url.visits(new_index)),
        ui::PageTransitionFromInt(new_url.visit_transitions(new_index))));
  }
}


// static
void TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& typed_url, history::URLRow* new_url) {
  DCHECK_GT(typed_url.visits_size(), 0);
  CHECK_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  new_url->set_title(base::UTF8ToUTF16(typed_url.title()));
  new_url->set_hidden(typed_url.hidden());
  // Only provide the initial value for the last_visit field - after that, let
  // the history code update the last_visit field on its own.
  if (new_url->last_visit().is_null()) {
    new_url->set_last_visit(base::Time::FromInternalValue(
        typed_url.visits(typed_url.visits_size() - 1)));
  }
}

bool TypedUrlModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  const syncer::ModelTypeSet encrypted_types = trans.GetEncryptedTypes();
  return !encrypted_types.Has(syncer::TYPED_URLS) ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
