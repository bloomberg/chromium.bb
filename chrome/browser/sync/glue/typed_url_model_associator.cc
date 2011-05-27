// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_model_associator.h"

#include <algorithm>
#include <set>

#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

namespace browser_sync {

const char kTypedUrlTag[] = "google_chrome_typed_urls";

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

bool TypedUrlModelAssociator::AssociateModels() {
  VLOG(1) << "Associating TypedUrl Models";
  DCHECK(expected_loop_ == MessageLoop::current());

  std::vector<history::URLRow> typed_urls;
  if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
    LOG(ERROR) << "Could not get the typed_url entries.";
    return false;
  }

  // Get all the visits.
  std::map<history::URLID, history::VisitVector> visit_vectors;
  for (std::vector<history::URLRow>::iterator ix = typed_urls.begin();
       ix != typed_urls.end(); ++ix) {
    if (!history_backend_->GetVisitsForURL(ix->id(),
                                           &(visit_vectors[ix->id()]))) {
      LOG(ERROR) << "Could not get the url's visits.";
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
  }

  TypedUrlTitleVector titles;
  TypedUrlVector new_urls;
  TypedUrlVisitVector new_visits;
  TypedUrlUpdateVector updated_urls;

  {
    sync_api::WriteTransaction trans(sync_service_->GetUserShare());
    sync_api::ReadNode typed_url_root(&trans);
    if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
      LOG(ERROR) << "Server did not create the top-level typed_url node. We "
                 << "might be running against an out-of-date server.";
      return false;
    }

    std::set<std::string> current_urls;
    for (std::vector<history::URLRow>::iterator ix = typed_urls.begin();
         ix != typed_urls.end(); ++ix) {
      std::string tag = ix->url().spec();

      history::VisitVector& visits = visit_vectors[ix->id()];

      sync_api::ReadNode node(&trans);
      if (node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
        const sync_pb::TypedUrlSpecifics& typed_url(
            node.GetTypedUrlSpecifics());
        DCHECK_EQ(tag, typed_url.url());

        history::URLRow new_url(ix->url());

        std::vector<base::Time> added_visits;
        int difference = MergeUrls(typed_url, *ix, &visits, &new_url,
                                   &added_visits);
        if (difference & DIFF_NODE_CHANGED) {
          sync_api::WriteNode write_node(&trans);
          if (!write_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
            LOG(ERROR) << "Failed to edit typed_url sync node.";
            return false;
          }
          // We don't want to resurrect old visits that have been aged out by
          // other clients, so remove all visits that are older than the
          // earliest existing visit in the sync node.
          if (typed_url.visit_size() > 0) {
            base::Time earliest_visit =
                base::Time::FromInternalValue(typed_url.visit(0));
            for (history::VisitVector::iterator it = visits.begin();
                 it != visits.end() && it->visit_time < earliest_visit; ) {
              it = visits.erase(it);
            }
            // Should never be possible to delete all the items, since the
            // visit vector contains all the items in typed_url.visits.
            DCHECK(visits.size() > 0);
          } else {
            NOTREACHED() << "Syncing typed URL with no visits: " <<
                typed_url.url();
          }
          WriteToSyncNode(new_url, visits, &write_node);
        }
        if (difference & DIFF_TITLE_CHANGED) {
          titles.push_back(std::pair<GURL, string16>(new_url.url(),
                                                     new_url.title()));
        }
        if (difference & DIFF_ROW_CHANGED) {
          updated_urls.push_back(
              std::pair<history::URLID, history::URLRow>(ix->id(), new_url));
        }
        if (difference & DIFF_VISITS_ADDED) {
          new_visits.push_back(
              std::pair<GURL, std::vector<base::Time> >(ix->url(),
                                                        added_visits));
        }

        Associate(&tag, node.GetId());
      } else {
        // Sync has never seen this URL before.
        sync_api::WriteNode node(&trans);
        if (!node.InitUniqueByCreation(syncable::TYPED_URLS,
                                       typed_url_root, tag)) {
          LOG(ERROR) << "Failed to create typed_url sync node.";
          return false;
        }

        node.SetTitle(UTF8ToWide(tag));
        WriteToSyncNode(*ix, visits, &node);

        Associate(&tag, node.GetId());
      }

      current_urls.insert(tag);
    }

    int64 sync_child_id = typed_url_root.GetFirstChildId();
    while (sync_child_id != sync_api::kInvalidId) {
      sync_api::ReadNode sync_child_node(&trans);
      if (!sync_child_node.InitByIdLookup(sync_child_id)) {
        LOG(ERROR) << "Failed to fetch child node.";
        return false;
      }
      const sync_pb::TypedUrlSpecifics& typed_url(
        sync_child_node.GetTypedUrlSpecifics());

      if (current_urls.find(typed_url.url()) == current_urls.end()) {
        new_visits.push_back(
            std::pair<GURL, std::vector<base::Time> >(
                GURL(typed_url.url()),
                std::vector<base::Time>()));
        std::vector<base::Time>& visits = new_visits.back().second;
        history::URLRow new_url = TypedUrlSpecificsToURLRow(typed_url);

        // The latest visit gets added automatically, so skip it.
        for (int c = 0; c < typed_url.visit_size() - 1; ++c) {
          DCHECK(typed_url.visit(c) < typed_url.visit(c + 1));
          visits.push_back(base::Time::FromInternalValue(typed_url.visit(c)));
        }

        Associate(&typed_url.url(), sync_child_node.GetId());
        new_urls.push_back(new_url);
      }

      sync_child_id = sync_child_node.GetSuccessorId();
    }
  }

  // Since we're on the history thread, we don't have to worry about updating
  // the history database after closing the write transaction, since
  // this is the only thread that writes to the database.  We also don't have
  // to worry about the sync model getting out of sync, because changes are
  // propagated to the ChangeProcessor on this thread.
  return WriteToHistoryBackend(&titles, &new_urls, &updated_urls,
                               &new_visits, NULL);
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

bool TypedUrlModelAssociator::DisassociateModels() {
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
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());

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
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
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
int TypedUrlModelAssociator::MergeUrls(
    const sync_pb::TypedUrlSpecifics& typed_url,
    const history::URLRow& url,
    history::VisitVector* visits,
    history::URLRow* new_url,
    std::vector<base::Time>* new_visits) {
  DCHECK(new_url);
  DCHECK(!typed_url.url().compare(url.url().spec()));
  DCHECK(!typed_url.url().compare(new_url->url().spec()));
  DCHECK(visits->size());

  int original_visit_count = visits->size();

  // Convert these values only once.
  string16 typed_title(UTF8ToUTF16(typed_url.title()));
  base::Time typed_visit =
      base::Time::FromInternalValue(
          typed_url.visit(typed_url.visit_size() - 1));


  // This is a bitfield representing what we'll need to update with the output
  // value.
  int different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((typed_title.compare(url.title()) != 0) ||
      (typed_url.hidden() != url.hidden())) {
    // Use the values from the most recent visit.
    if (typed_visit >= url.last_visit()) {
      new_url->set_title(typed_title);
      new_url->set_hidden(typed_url.hidden());
      different |= DIFF_ROW_CHANGED;

      // If we're changing the local title, note this.
      if (new_url->title().compare(url.title()) != 0) {
        different |= DIFF_TITLE_CHANGED;
      }
    } else {
      new_url->set_title(url.title());
      new_url->set_hidden(url.hidden());
      different |= DIFF_NODE_CHANGED;
    }
  } else {
    // No difference.
    new_url->set_title(url.title());
    new_url->set_hidden(url.hidden());
  }

  // For typed count, we just select the maximum value. This is not technically
  // correct since it undercounts URLs that have been typed on multiple systems
  // between syncs, but it's the best we can do.
  if (typed_url.typed_count() > url.typed_count()) {
    new_url->set_typed_count(typed_url.typed_count());
    different |= DIFF_ROW_CHANGED;
  } else if (typed_url.typed_count() < url.typed_count()) {
    new_url->set_typed_count(url.typed_count());
    different |= DIFF_NODE_CHANGED;
  } else {
    // No difference.
    new_url->set_typed_count(typed_url.typed_count());
  }

  size_t left_num_visits = typed_url.visit_size();
  size_t right_num_visits = visits->size();
  size_t left = 0;
  size_t right = 0;
  // Walk through the two sets of visits and figure out if any new visits were
  // added on either side (left = sync node, right = history db entry).
  while (left < left_num_visits || right < right_num_visits) {
    // Time objects are initialized to "earliest possible time".
    base::Time left_time, right_time;
    if (left < left_num_visits)
      left_time = base::Time::FromInternalValue(typed_url.visit(left));
    if (right < right_num_visits)
      right_time = (*visits)[right].visit_time;
    if (left >= left_num_visits ||
        (right < right_num_visits && left_time > right_time)) {
      // We found a visit in the history DB that doesn't exist in the sync DB,
      // so mark the node as modified so the caller will update the sync node.
      different |= DIFF_NODE_CHANGED;
      ++right;
    } else if (right >= right_num_visits || left_time < right_time) {
      // Found a visit in the sync node that doesn't exist in the history DB, so
      // add it to our list of new visits and set the appropriate flag so the
      // caller will update the history DB.
      different |= DIFF_VISITS_ADDED;
      new_visits->push_back(left_time);
      // This visit is added to visits below.
      ++left;
    } else {
      // Same (already synced) entry found in both DBs - no need to do anything.
      ++left;
      ++right;
    }
  }

  if (different & DIFF_VISITS_ADDED) {
    // Insert new visits into the apropriate place in the visits vector.
    history::VisitVector::iterator visit_ix = visits->begin();
    for (std::vector<base::Time>::iterator new_visit = new_visits->begin();
         new_visit != new_visits->end(); ++new_visit) {
      while (visit_ix != visits->end() && *new_visit > visit_ix->visit_time) {
        ++visit_ix;
      }
      visit_ix = visits->insert(visit_ix,
                                history::VisitRow(url.id(), *new_visit,
                                                  0, 0, 0));
      ++visit_ix;
    }
  }

  new_url->set_last_visit(visits->back().visit_time);

  // visit_count is a value that is only loosely correlated with the visit
  // array (for example, reloading a page does not increment the visit_count
  // but does register a visit in the visit vector). To determine the merged
  // visit_count, we'll calculate the largest diff between visit_count and the
  // sizes of the visit arrays and then apply that to the final visit array.
  // This is inexact, but the exact value isn't particularly important (it's
  // just used to help prioritize between various autocomplete suggestions, so
  // in general just the magnitude is important).
  int typed_url_visit_delta =
      typed_url.visit_size() - typed_url.visited_count();
  int history_url_visit_delta =
      original_visit_count - url.visit_count();
  if (std::abs(typed_url_visit_delta) > std::abs(history_url_visit_delta))
    new_url->set_visit_count(visits->size() - typed_url_visit_delta);
  else
    new_url->set_visit_count(visits->size() - history_url_visit_delta);

  if (new_url->visit_count() != url.visit_count())
    different |= DIFF_ROW_CHANGED;
  if (new_url->visit_count() != typed_url.visited_count())
    different |= DIFF_NODE_CHANGED;

  return different;
}

// static
void TypedUrlModelAssociator::WriteToSyncNode(
    const history::URLRow& url,
    const history::VisitVector& visits,
    sync_api::WriteNode* node) {
  DCHECK(!url.last_visit().is_null());
  DCHECK(!visits.empty());
  DCHECK(url.last_visit() == visits.back().visit_time);

  sync_pb::TypedUrlSpecifics typed_url;
  typed_url.set_url(url.url().spec());
  typed_url.set_title(UTF16ToUTF8(url.title()));
  typed_url.set_typed_count(url.typed_count());
  typed_url.set_hidden(url.hidden());
  typed_url.set_visited_count(url.visit_count());

  for (history::VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    typed_url.add_visit(visit->visit_time.ToInternalValue());
  }

  node->SetTypedUrlSpecifics(typed_url);
}

// static
void TypedUrlModelAssociator::DiffVisits(
    const history::VisitVector& old_visits,
    const sync_pb::TypedUrlSpecifics& new_url,
    std::vector<base::Time>* new_visits,
    history::VisitVector* removed_visits) {
  size_t left_visit_count = old_visits.size();
  size_t right_visit_count = new_url.visit_size();
  size_t left = 0;
  size_t right = 0;
  while (left < left_visit_count && right < right_visit_count) {
    base::Time right_time = base::Time::FromInternalValue(new_url.visit(right));
    if (old_visits[left].visit_time < right_time) {
      removed_visits->push_back(old_visits[left]);
      ++left;
    } else if (old_visits[left].visit_time > right_time) {
      new_visits->push_back(right_time);
      ++right;
    } else {
      ++left;
      ++right;
    }
  }

  for ( ; left < left_visit_count; ++left) {
    removed_visits->push_back(old_visits[left]);
  }

  for ( ; right < right_visit_count; ++right) {
    new_visits->push_back(base::Time::FromInternalValue(new_url.visit(right)));
  }
}


// static
history::URLRow TypedUrlModelAssociator::TypedUrlSpecificsToURLRow(
    const sync_pb::TypedUrlSpecifics& typed_url) {
  DCHECK(typed_url.visit_size() > 0);
  history::URLRow new_url(GURL(typed_url.url()));
  new_url.set_title(UTF8ToUTF16(typed_url.title()));
  new_url.set_visit_count(typed_url.visited_count());
  new_url.set_typed_count(typed_url.typed_count());
  new_url.set_hidden(typed_url.hidden());
  new_url.set_last_visit(base::Time::FromInternalValue(
      typed_url.visit(typed_url.visit_size() - 1)));
  return new_url;
}

bool TypedUrlModelAssociator::CryptoReadyIfNecessary() {
  // We only access the cryptographer while holding a transaction.
  sync_api::ReadTransaction trans(sync_service_->GetUserShare());
  syncable::ModelTypeSet encrypted_types;
  sync_service_->GetEncryptedDataTypes(&encrypted_types);
  return encrypted_types.count(syncable::TYPED_URLS) == 0 ||
         sync_service_->IsCryptographerReady(&trans);
}

}  // namespace browser_sync
