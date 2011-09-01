// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_model_associator.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/tracked.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/read_transaction.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

namespace browser_sync {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

const char kTypedUrlTag[] = "google_chrome_typed_urls";

static bool CheckVisitOrdering(const history::VisitVector& visits) {
  int64 previous_visit_time = 0;
  for (history::VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    if (visit != visits.begin() &&
        previous_visit_time >= visit->visit_time.ToInternalValue()) {
      return false;
    }
    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

TypedUrlModelAssociator::TypedUrlModelAssociator(
    ProfileSyncService* sync_service,
    history::HistoryBackend* history_backend)
    : sync_service_(sync_service),
      history_backend_(history_backend),
      typed_url_node_id_(sync_api::kInvalidId),
      expected_loop_(MessageLoop::current()) {
  DCHECK(sync_service_);
  DCHECK(history_backend_);
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
}

TypedUrlModelAssociator::~TypedUrlModelAssociator() {}

bool TypedUrlModelAssociator::AssociateModels(SyncError* error) {
  VLOG(1) << "Associating TypedUrl Models";
  DCHECK(expected_loop_ == MessageLoop::current());

  std::vector<history::URLRow> typed_urls;
  if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
    error->Reset(FROM_HERE,
                "Could not get the typed_url entries.",
                model_type());
    return false;
  }

  // Get all the visits.
  std::map<history::URLID, history::VisitVector> visit_vectors;
  for (std::vector<history::URLRow>::iterator ix = typed_urls.begin();
       ix != typed_urls.end(); ++ix) {
    if (!history_backend_->GetVisitsForURL(ix->id(),
                                           &(visit_vectors[ix->id()]))) {
      error->Reset(FROM_HERE, "Could not get the url's visits.", model_type());
      return false;
    }
    // Sometimes (due to a bug elsewhere in the history or sync code, or due to
    // a crash between adding a URL to the history database and updating the
    // visit DB) the visit vector for a URL can be empty. If this happens, just
    // create a new visit whose timestamp is the same as the last_visit time.
    // This is a workaround for http://crbug.com/84258.
    if (visit_vectors[ix->id()].empty()) {
      history::VisitRow visit(
          ix->id(), ix->last_visit(), 0, PageTransition::TYPED, 0);
      visit_vectors[ix->id()].push_back(visit);
    }
    // Checking DB consistency to try to track down http://crbug.com/94733 - if
    // we start hitting this DCHECK, we can try to fixup the data by adding our
    // own mock visit as we do for empty visit vectors.
    DCHECK_EQ(ix->last_visit().ToInternalValue(),
              visit_vectors[ix->id()].back().visit_time.ToInternalValue());
    DCHECK(CheckVisitOrdering(visit_vectors[ix->id()]));
  }

  TypedUrlTitleVector titles;
  TypedUrlVector new_urls;
  TypedUrlVisitVector new_visits;
  TypedUrlUpdateVector updated_urls;

  {
    sync_api::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    sync_api::ReadNode typed_url_root(&trans);
    if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
      error->Reset(FROM_HERE,
                  "Server did not create the top-level typed_url node. We "
                  "might be running against an out-of-date server.",
                  model_type());
      return false;
    }

    std::set<std::string> current_urls;
    for (std::vector<history::URLRow>::iterator ix = typed_urls.begin();
         ix != typed_urls.end(); ++ix) {
      std::string tag = ix->url().spec();

      history::VisitVector& visits = visit_vectors[ix->id()];

      sync_api::ReadNode node(&trans);
      if (node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
        // Same URL exists in sync data and in history data - compare the
        // entries to see if there's any difference.
        const sync_pb::TypedUrlSpecifics& typed_url(
            node.GetTypedUrlSpecifics());
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
          sync_api::WriteNode write_node(&trans);
          if (!write_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
            error->Reset(FROM_HERE,
                         "Failed to edit typed_url sync node.",
                         model_type());
            return false;
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
        if (difference & DIFF_LOCAL_TITLE_CHANGED) {
          titles.push_back(std::pair<GURL, string16>(new_url.url(),
                                                     new_url.title()));
        }
        if (difference & DIFF_LOCAL_ROW_CHANGED) {
          updated_urls.push_back(
              std::pair<history::URLID, history::URLRow>(ix->id(), new_url));
        }
        if (difference & DIFF_LOCAL_VISITS_ADDED) {
          new_visits.push_back(
              std::pair<GURL, std::vector<history::VisitInfo> >(ix->url(),
                                                                added_visits));
        }

        Associate(&tag, node.GetId());
      } else {
        // Sync has never seen this URL before.
        sync_api::WriteNode node(&trans);
        if (!node.InitUniqueByCreation(syncable::TYPED_URLS,
                                       typed_url_root, tag)) {
          error->Reset(FROM_HERE,
                       "Failed to create typed_url sync node.",
                       model_type());
          return false;
        }

        node.SetTitle(UTF8ToWide(tag));
        WriteToSyncNode(*ix, visits, &node);

        Associate(&tag, node.GetId());
      }

      current_urls.insert(tag);
    }

    // Now walk the sync nodes and detect any URLs that exist there, but not in
    // the history DB, so we can add them to our local history DB.
    std::vector<int64> obsolete_nodes;
    int64 sync_child_id = typed_url_root.GetFirstChildId();
    while (sync_child_id != sync_api::kInvalidId) {
      sync_api::ReadNode sync_child_node(&trans);
      if (!sync_child_node.InitByIdLookup(sync_child_id)) {
        error->Reset(FROM_HERE, "Failed to fetch child node.", model_type());
        return false;
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
        VLOG(1) << "Deleting obsolete sync node with no visit transition info.";
        obsolete_nodes.push_back(sync_child_node.GetId());
        continue;
      }

      if (current_urls.find(typed_url.url()) == current_urls.end()) {
        new_visits.push_back(
            std::pair<GURL, std::vector<history::VisitInfo> >(
                GURL(typed_url.url()),
                std::vector<history::VisitInfo>()));
        std::vector<history::VisitInfo>& visits = new_visits.back().second;
        history::URLRow new_url(GURL(typed_url.url()));
        TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
            typed_url, &new_url);

        for (int c = 0; c < typed_url.visits_size(); ++c) {
          DCHECK(c == 0 || typed_url.visits(c) > typed_url.visits(c - 1));
          DCHECK_LE(typed_url.visit_transitions(c) & PageTransition::CORE_MASK,
                    PageTransition::LAST_CORE);
          visits.push_back(history::VisitInfo(
              base::Time::FromInternalValue(typed_url.visits(c)),
              static_cast<PageTransition::Type>(
                  typed_url.visit_transitions(c))));
        }

        Associate(&typed_url.url(), sync_child_node.GetId());
        new_urls.push_back(new_url);
      }
    }

    // If we encountered any obsolete nodes, remove them so they don't hang
    // around and confuse people looking at the sync node browser.
    if (!obsolete_nodes.empty()) {
      for (std::vector<int64>::const_iterator it = obsolete_nodes.begin();
           it != obsolete_nodes.end();
           ++it) {
        sync_api::WriteNode sync_node(&trans);
        if (!sync_node.InitByIdLookup(*it)) {
          error->Reset(FROM_HERE,
                      "Failed to fetch obsolete node.",
                      model_type());
          return false;
        }
        sync_node.Remove();
      }
    }
  }

  // Since we're on the history thread, we don't have to worry about updating
  // the history database after closing the write transaction, since
  // this is the only thread that writes to the database.  We also don't have
  // to worry about the sync model getting out of sync, because changes are
  // propagated to the ChangeProcessor on this thread.
  if (!WriteToHistoryBackend(&titles, &new_urls, &updated_urls,
                             &new_visits, NULL)) {
    error->Reset(FROM_HERE,
                 "Failed to write to history backend",
                 model_type());
    return false;
  }
  return true;
}

bool TypedUrlModelAssociator::DeleteAllNodes(
    sync_api::WriteTransaction* trans) {
  // TODO(sync): Add code to make this an explicit "delete" command rather than
  // a list of nodes to avoid having to store tombstones on the server
  // (http://crbug.com/80179).
  DCHECK(expected_loop_ == MessageLoop::current());
  for (TypedUrlToSyncIdMap::iterator node_id = id_map_.begin();
       node_id != id_map_.end(); ++node_id) {
    sync_api::WriteNode sync_node(trans);
    if (!sync_node.InitByIdLookup(node_id->second)) {
      LOG(ERROR) << "Typed url node lookup failed.";
      return false;
    }
    sync_node.Remove();
  }

  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool TypedUrlModelAssociator::DisassociateModels(SyncError* error) {
  id_map_.clear();
  id_map_inverse_.clear();
  return true;
}

bool TypedUrlModelAssociator::SyncModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  *has_nodes = false;
  int64 typed_url_sync_id;
  if (!GetSyncIdForTaggedNode(kTypedUrlTag, &typed_url_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level typed_url node. We "
               << "might be running against an out-of-date server.";
    return false;
  }
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());

  sync_api::ReadNode typed_url_node(&trans);
  if (!typed_url_node.InitByIdLookup(typed_url_sync_id)) {
    LOG(ERROR) << "Server did not create the top-level typed_url node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  // The sync model has user created nodes if the typed_url folder has any
  // children.
  *has_nodes = sync_api::kInvalidId != typed_url_node.GetFirstChildId();
  return true;
}

void TypedUrlModelAssociator::AbortAssociation() {
  // TODO(atwilson): Implement this.
}

const std::string* TypedUrlModelAssociator::GetChromeNodeFromSyncId(
    int64 sync_id) {
  return NULL;
}

bool TypedUrlModelAssociator::InitSyncNodeFromChromeId(
    const std::string& node_id,
    sync_api::BaseNode* sync_node) {
  return false;
}

int64 TypedUrlModelAssociator::GetSyncIdFromChromeId(
    const std::string& typed_url) {
  TypedUrlToSyncIdMap::const_iterator iter = id_map_.find(typed_url);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void TypedUrlModelAssociator::Associate(
    const std::string* typed_url, int64 sync_id) {
  DCHECK(expected_loop_ == MessageLoop::current());
  DCHECK(!IsAssociated(*typed_url));
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[*typed_url] = sync_id;
  id_map_inverse_[sync_id] = *typed_url;
}

bool TypedUrlModelAssociator::IsAssociated(const std::string& typed_url) {
  DCHECK(expected_loop_ == MessageLoop::current());
  return id_map_.find(typed_url) != id_map_.end();
}

void TypedUrlModelAssociator::Disassociate(int64 sync_id) {
  DCHECK(expected_loop_ == MessageLoop::current());
  SyncIdToTypedUrlMap::iterator iter = id_map_inverse_.find(sync_id);
  if (iter == id_map_inverse_.end())
    return;
  CHECK(id_map_.erase(iter->second));
  id_map_inverse_.erase(iter);
}

bool TypedUrlModelAssociator::GetSyncIdForTaggedNode(const std::string& tag,
                                                     int64* sync_id) {
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

bool TypedUrlModelAssociator::WriteToHistoryBackend(
    const TypedUrlTitleVector* titles,
    const TypedUrlVector* new_urls,
    const TypedUrlUpdateVector* updated_urls,
    const TypedUrlVisitVector* new_visits,
    const history::VisitVector* deleted_visits) {
  if (titles) {
    for (TypedUrlTitleVector::const_iterator title = titles->begin();
         title != titles->end(); ++title) {
      history_backend_->SetPageTitle(title->first, title->second);
    }
  }
  if (new_urls) {
#ifndef NDEBUG
    // All of these URLs should already have been associated.
    for (TypedUrlVector::const_iterator url = new_urls->begin();
         url != new_urls->end(); ++url) {
      DCHECK(IsAssociated(url->url().spec()));
    }
#endif
    history_backend_->AddPagesWithDetails(*new_urls, history::SOURCE_SYNCED);
  }
  if (updated_urls) {
    for (TypedUrlUpdateVector::const_iterator url = updated_urls->begin();
         url != updated_urls->end(); ++url) {
      // Caller should have already initialized the visit and typed counts
      // appropriately to match what the values should have been before adding
      // visits.
      DCHECK(url->second.visit_count());
      DCHECK(url->second.typed_count());
      DCHECK(IsAssociated(url->second.url().spec()));
      if (!history_backend_->UpdateURL(url->first, url->second)) {
        LOG(ERROR) << "Could not update page: " << url->second.url().spec();
        return false;
      }
    }
  }
  if (new_visits) {
    for (TypedUrlVisitVector::const_iterator visits = new_visits->begin();
         visits != new_visits->end(); ++visits) {
      DCHECK(IsAssociated(visits->first.spec()));
      if (!history_backend_->AddVisits(visits->first, visits->second,
                                       history::SOURCE_SYNCED)) {
        LOG(ERROR) << "Could not add visits.";
        return false;
      }
    }
  }
  if (deleted_visits) {
    if (!history_backend_->RemoveVisits(*deleted_visits)) {
      LOG(ERROR) << "Could not remove visits.";
      return false;
    }
  }
  return true;
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
  // visit_transitions arrays to the protobuf), just overwrite
  // it with our local history data.
  if (node.visits_size() == 0)
    return DIFF_UPDATE_NODE;

  // Convert these values only once.
  string16 node_title(UTF8ToUTF16(node.title()));
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

      // If we're changing the local title, note this.
      if (new_url->title().compare(url.title()) != 0) {
        different |= DIFF_LOCAL_TITLE_CHANGED;
      }
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
      different |= DIFF_LOCAL_VISITS_ADDED;
      new_visits->push_back(history::VisitInfo(
          node_time, node.visit_transitions(node_visit_index)));
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
    sync_api::WriteNode* node) {
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
  typed_url->set_title(UTF16ToUTF8(url.title()));
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
      PageTransition::Type transition = static_cast<PageTransition::Type>(
          visit->transition) & PageTransition::CORE_MASK;
      // We ignore reload visits.
      if (transition == PageTransition::RELOAD)
        continue;
      ++total;
      if (transition == PageTransition::TYPED)
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
    PageTransition::Type transition = static_cast<PageTransition::Type>(
        visit->transition) & PageTransition::CORE_MASK;
    // Skip reload visits.
    if (transition == PageTransition::RELOAD)
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed && transition != PageTransition::TYPED)
      continue;

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed || transition != PageTransition::TYPED) {
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
    typed_url->add_visit_transitions(PageTransition::RELOAD);
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
  size_t old_visit_count = old_visits.size();
  size_t new_visit_count = new_url.visits_size();
  size_t old_index = 0;
  size_t new_index = 0;
  while (old_index < old_visit_count && new_index < new_visit_count) {
    base::Time new_visit_time =
        base::Time::FromInternalValue(new_url.visits(new_index));
    if (old_visits[old_index].visit_time < new_visit_time) {
      removed_visits->push_back(old_visits[old_index]);
      ++old_index;
    } else if (old_visits[old_index].visit_time > new_visit_time) {
      new_visits->push_back(history::VisitInfo(
          new_visit_time, new_url.visit_transitions(new_index)));
      ++new_index;
    } else {
      ++old_index;
      ++new_index;
    }
  }

  for ( ; old_index < old_visit_count; ++old_index) {
    removed_visits->push_back(old_visits[old_index]);
  }

  for ( ; new_index < new_visit_count; ++new_index) {
    new_visits->push_back(history::VisitInfo(
        base::Time::FromInternalValue(new_url.visits(new_index)),
        new_url.visit_transitions(new_index)));
  }
}


// static
void TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
    const sync_pb::TypedUrlSpecifics& typed_url, history::URLRow* new_url) {
  DCHECK_GT(typed_url.visits_size(), 0);
  CHECK_EQ(typed_url.visit_transitions_size(), typed_url.visits_size());
  new_url->set_title(UTF8ToUTF16(typed_url.title()));
  new_url->set_hidden(typed_url.hidden());
  new_url->set_last_visit(base::Time::FromInternalValue(
      typed_url.visits(typed_url.visits_size() - 1)));
}

bool TypedUrlModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  syncable::ModelTypeSet encrypted_types;
  encrypted_types = sync_api::GetEncryptedTypes(&trans);
  return encrypted_types.count(syncable::TYPED_URLS) == 0 ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
