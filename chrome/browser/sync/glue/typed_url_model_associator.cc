// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_model_associator.h"

#include <set>

#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

namespace browser_sync {

const char kTypedUrlTag[] = "google_chrome_typed_urls";

TypedUrlModelAssociator::TypedUrlModelAssociator(
    ProfileSyncService* sync_service,
    history::HistoryBackend* history_backend,
    UnrecoverableErrorHandler* error_handler)
    : sync_service_(sync_service),
      history_backend_(history_backend),
      error_handler_(error_handler),
      typed_url_node_id_(sync_api::kInvalidId),
      expected_loop_(MessageLoop::current()) {
  DCHECK(sync_service_);
  DCHECK(history_backend_);
  DCHECK(error_handler_);
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::UI));
}

bool TypedUrlModelAssociator::AssociateModels() {
  LOG(INFO) << "Associating TypedUrl Models";
  DCHECK(expected_loop_ == MessageLoop::current());

  std::vector<history::URLRow> typed_urls;
  if (!history_backend_->GetAllTypedURLs(&typed_urls)) {
    LOG(ERROR) << "Could not get the typed_url entries.";
    return false;
  }

  sync_api::WriteTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode typed_url_root(&trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    LOG(ERROR) << "Server did not create the top-level typed_url node. We "
               << "might be running against an out-of-date server.";
    return false;
  }

  std::set<std::string> current_urls;
  TypedUrlTitleVector titles;
  TypedUrlVector new_urls;
  TypedUrlUpdateVector updated_urls;

  for (std::vector<history::URLRow>::iterator ix = typed_urls.begin();
       ix != typed_urls.end(); ++ix) {
    std::string tag = ix->url().spec();

    sync_api::ReadNode node(&trans);
    if (node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
      const sync_pb::TypedUrlSpecifics& typed_url(node.GetTypedUrlSpecifics());
      DCHECK_EQ(tag, typed_url.url());

      history::URLRow new_url(ix->url());

      int difference = MergeUrls(typed_url, *ix, &new_url);
      if (difference & DIFF_NODE_CHANGED) {
        sync_api::WriteNode write_node(&trans);
        if (!write_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
          LOG(ERROR) << "Failed to edit typed_url sync node.";
          return false;
        }
        WriteToSyncNode(new_url, &write_node);
      }
      if (difference & DIFF_TITLE_CHANGED) {
        titles.push_back(std::pair<GURL, std::wstring>(new_url.url(),
                                                       new_url.title()));
      }
      if (difference & DIFF_ROW_CHANGED) {
        updated_urls.push_back(
            std::pair<history::URLID, history::URLRow>(ix->id(), new_url));
      }

      Associate(&tag, node.GetId());
    } else {
      sync_api::WriteNode node(&trans);
      if (!node.InitUniqueByCreation(syncable::TYPED_URLS,
                                     typed_url_root, tag)) {
        LOG(ERROR) << "Failed to create typed_url sync node.";
        return false;
      }

      node.SetTitle(UTF8ToWide(tag));
      WriteToSyncNode(*ix, &node);

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
      history::URLRow new_url(GURL(typed_url.url()));

      new_url.set_title(UTF8ToWide(typed_url.title()));
      new_url.set_visit_count(typed_url.visit_count());
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_last_visit(
          base::Time::FromInternalValue(typed_url.last_visit()));
      new_url.set_hidden(typed_url.hidden());

      Associate(&typed_url.url(), sync_child_node.GetId());
      new_urls.push_back(new_url);
    }

    sync_child_id = sync_child_node.GetSuccessorId();
  }

  WriteToHistoryBackend(&titles, &new_urls, &updated_urls);

  return true;
}

bool TypedUrlModelAssociator::DeleteAllNodes(
    sync_api::WriteTransaction* trans) {
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
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());

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

bool TypedUrlModelAssociator::ChromeModelHasUserCreatedNodes(bool* has_nodes) {
  DCHECK(has_nodes);
  // Assume the typed_url model always have user-created nodes.
  *has_nodes = true;
  return true;
}

int64 TypedUrlModelAssociator::GetSyncIdFromChromeId(
    const std::string typed_url) {
  TypedUrlToSyncIdMap::const_iterator iter = id_map_.find(typed_url);
  return iter == id_map_.end() ? sync_api::kInvalidId : iter->second;
}

void TypedUrlModelAssociator::Associate(
    const std::string* typed_url, int64 sync_id) {
  DCHECK(expected_loop_ == MessageLoop::current());
  DCHECK_NE(sync_api::kInvalidId, sync_id);
  DCHECK(id_map_.find(*typed_url) == id_map_.end());
  DCHECK(id_map_inverse_.find(sync_id) == id_map_inverse_.end());
  id_map_[*typed_url] = sync_id;
  id_map_inverse_[sync_id] = *typed_url;
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
  sync_api::ReadTransaction trans(
      sync_service_->backend()->GetUserShareHandle());
  sync_api::ReadNode sync_node(&trans);
  if (!sync_node.InitByTagLookup(tag.c_str()))
    return false;
  *sync_id = sync_node.GetId();
  return true;
}

void TypedUrlModelAssociator::WriteToHistoryBackend(
    const TypedUrlTitleVector* titles,
    const TypedUrlVector* new_urls,
    const TypedUrlUpdateVector* updated_urls) {
  if (titles) {
    for (TypedUrlTitleVector::const_iterator title = titles->begin();
         title != titles->end(); ++title) {
      history_backend_->SetPageTitle(title->first, title->second);
    }
  }
  if (new_urls) {
    history_backend_->AddPagesWithDetails(*new_urls);
  }
  if (updated_urls) {
    for (TypedUrlUpdateVector::const_iterator url = updated_urls->begin();
         url != updated_urls->end(); ++url) {
      history_backend_->UpdateURL(url->first, url->second);
    }
  }
}

// static
int TypedUrlModelAssociator::MergeUrls(
    const sync_pb::TypedUrlSpecifics& typed_url,
    const history::URLRow& url,
    history::URLRow* new_url) {
  DCHECK(new_url);
  DCHECK(!typed_url.url().compare(url.url().spec()));
  DCHECK(!typed_url.url().compare(new_url->url().spec()));

  // Convert these values only once.
  std::wstring typed_title(UTF8ToWide(typed_url.title()));
  base::Time typed_visit =
      base::Time::FromInternalValue(typed_url.last_visit());

  // This is a bitfield represting what we'll need to update with the output
  // value.
  int different = DIFF_NONE;

  // Check if the non-incremented values changed.
  if ((typed_title.compare(url.title()) != 0) ||
      (typed_visit != url.last_visit()) ||
      (typed_url.hidden() != url.hidden())) {

    // Use the values from the most recent visit.
    if (typed_visit >= url.last_visit()) {
      new_url->set_title(typed_title);
      new_url->set_last_visit(typed_visit);
      new_url->set_hidden(typed_url.hidden());
      different |= DIFF_ROW_CHANGED;

      // If we're changing the local title, note this.
      if (new_url->title().compare(url.title()) != 0) {
        different |= DIFF_TITLE_CHANGED;
      }
    } else {
      new_url->set_title(url.title());
      new_url->set_last_visit(url.last_visit());
      new_url->set_hidden(url.hidden());
      different |= DIFF_NODE_CHANGED;
    }
  } else {
    // No difference.
    new_url->set_title(url.title());
    new_url->set_last_visit(url.last_visit());
    new_url->set_hidden(url.hidden());
  }

  // For visit count, we just select the maximum value.
  if (typed_url.visit_count() > url.visit_count()) {
    new_url->set_visit_count(typed_url.visit_count());
    different |= DIFF_ROW_CHANGED;
  } else if (typed_url.visit_count() < url.visit_count()) {
    new_url->set_visit_count(url.visit_count());
    different |= DIFF_NODE_CHANGED;
  } else {
    new_url->set_visit_count(typed_url.visit_count());
  }

  // For typed count, we just select the maximum value.
  if (typed_url.typed_count() > url.typed_count()) {
    new_url->set_typed_count(typed_url.typed_count());
    different |= DIFF_ROW_CHANGED;
  } else if (typed_url.typed_count() < url.typed_count()) {
    new_url->set_typed_count(url.visit_count());
    different |= DIFF_NODE_CHANGED;
  } else {
    // No difference.
    new_url->set_typed_count(typed_url.typed_count());
  }

  return different;
}

// static
void TypedUrlModelAssociator::WriteToSyncNode(const history::URLRow& url,
                                              sync_api::WriteNode* node) {
  sync_pb::TypedUrlSpecifics typed_url;
  typed_url.set_url(url.url().spec());
  typed_url.set_title(WideToUTF8(url.title()));
  typed_url.set_visit_count(url.visit_count());
  typed_url.set_typed_count(url.typed_count());
  typed_url.set_last_visit(url.last_visit().ToInternalValue());
  typed_url.set_hidden(url.hidden());

  node->SetTypedUrlSpecifics(typed_url);
}

}  // namespace browser_sync
