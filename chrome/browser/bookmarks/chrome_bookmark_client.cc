// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmarks_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkPermanentNode;
using bookmarks::ManagedBookmarksTracker;

namespace {

void RunCallbackWithImage(
    const favicon_base::FaviconImageCallback& callback,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  favicon_base::FaviconImageResult result;
  if (bitmap_result.is_valid()) {
    result.image = gfx::Image::CreateFrom1xPNGBytes(
        bitmap_result.bitmap_data->front(), bitmap_result.bitmap_data->size());
    result.icon_url = bitmap_result.icon_url;
    callback.Run(result);
    return;
  }
  callback.Run(result);
}

void LoadInitialContents(BookmarkPermanentNode* node,
                         base::ListValue* initial_bookmarks,
                         int64* next_node_id) {
  // Load the initial contents of the |node| now, and assign it an unused ID.
  int64 id = *next_node_id;
  node->set_id(id);
  *next_node_id = ManagedBookmarksTracker::LoadInitial(
      node, initial_bookmarks, id + 1);
  node->set_visible(!node->empty());
}

}  // namespace

ChromeBookmarkClient::ChromeBookmarkClient(Profile* profile)
    : profile_(profile),
      history_service_(NULL),
      model_(NULL),
      managed_node_(NULL),
      supervised_node_(NULL) {
}

ChromeBookmarkClient::~ChromeBookmarkClient() {
}

void ChromeBookmarkClient::Init(BookmarkModel* model) {
  DCHECK(model);
  DCHECK(!model_);
  model_ = model;
  model_->AddObserver(this);

  managed_bookmarks_tracker_.reset(new ManagedBookmarksTracker(
      model_,
      profile_->GetPrefs(),
      false,
      base::Bind(&ChromeBookmarkClient::GetManagedBookmarksDomain,
                 base::Unretained(this))));
  supervised_bookmarks_tracker_.reset(new ManagedBookmarksTracker(
      model_,
      profile_->GetPrefs(),
      true,
      base::Callback<std::string()>()));
}

void ChromeBookmarkClient::Shutdown() {
  favicon_changed_subscription_.reset();
  if (model_) {
    model_->RemoveObserver(this);
    model_ = NULL;
  }
  BookmarkClient::Shutdown();
}

bool ChromeBookmarkClient::PreferTouchIcon() {
#if !defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

base::CancelableTaskTracker::TaskId
ChromeBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
    const favicon_base::FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return base::CancelableTaskTracker::kBadTaskId;
  if (type == favicon_base::FAVICON) {
    return favicon_service->GetFaviconImageForPageURL(
        page_url, callback, tracker);
  } else {
    return favicon_service->GetRawFaviconForPageURL(
        page_url,
        type,
        0,
        base::Bind(&RunCallbackWithImage, callback),
        tracker);
  }
}

bool ChromeBookmarkClient::SupportsTypedCountForNodes() {
  return true;
}

void ChromeBookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  history::URLDatabase* url_db =
      history_service_ ? history_service_->InMemoryDatabase() : NULL;
  for (NodeSet::const_iterator i = nodes.begin(); i != nodes.end(); ++i) {
    int typed_count = 0;

    // If |url_db| is the InMemoryDatabase, it might not cache all URLRows, but
    // it guarantees to contain those with |typed_count| > 0. Thus, if we cannot
    // fetch the URLRow, it is safe to assume that its |typed_count| is 0.
    history::URLRow url;
    if (url_db && url_db->GetRowForURL((*i)->url(), &url))
      typed_count = url.typed_count();

    NodeTypedCountPair pair(*i, typed_count);
    node_typed_count_pairs->push_back(pair);
  }
}

bool ChromeBookmarkClient::IsPermanentNodeVisible(
    const BookmarkPermanentNode* node) {
  DCHECK(node->type() == BookmarkNode::BOOKMARK_BAR ||
         node->type() == BookmarkNode::OTHER_NODE ||
         node->type() == BookmarkNode::MOBILE ||
         node == managed_node_ ||
         node == supervised_node_);
  if (node == managed_node_ || node == supervised_node_)
    return false;
#if !defined(OS_IOS)
  return node->type() != BookmarkNode::MOBILE;
#else
  return node->type() == BookmarkNode::MOBILE;
#endif
}

void ChromeBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
  content::RecordAction(action);
}

bookmarks::LoadExtraCallback ChromeBookmarkClient::GetLoadExtraNodesCallback() {
  // Create the managed_node_ and supervised_node_ with a temporary ID of 0 now.
  // They will be populated (and assigned proper IDs) in the LoadExtraNodes
  // callback.
  // The ownership of managed_node_ and supervised_node_ is in limbo until
  // LoadExtraNodes runs, so we leave them in the care of the closure meanwhile.
  scoped_ptr<BookmarkPermanentNode> managed(new BookmarkPermanentNode(0));
  managed_node_ = managed.get();
  scoped_ptr<BookmarkPermanentNode> supervised(new BookmarkPermanentNode(0));
  supervised_node_ = supervised.get();

  return base::Bind(
      &ChromeBookmarkClient::LoadExtraNodes,
      base::Passed(&managed),
      base::Passed(managed_bookmarks_tracker_->GetInitialManagedBookmarks()),
      base::Passed(&supervised),
      base::Passed(
          supervised_bookmarks_tracker_->GetInitialManagedBookmarks()));
}

bool ChromeBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  // The |managed_node_| can have its title updated if the user signs in or
  // out, since the name of the managed domain can appear in it.
  // Also, both |managed_node_| and |supervised_node_| can have their title
  // updated on locale changes (crbug.com/459448).
  return (!bookmarks::IsDescendantOf(permanent_node, managed_node_) &&
          !bookmarks::IsDescendantOf(permanent_node, supervised_node_)) ||
         permanent_node == managed_node_ ||
         permanent_node == supervised_node_;
}

bool ChromeBookmarkClient::CanSyncNode(const BookmarkNode* node) {
  return !bookmarks::IsDescendantOf(node, managed_node_) &&
         !bookmarks::IsDescendantOf(node, supervised_node_);
}

bool ChromeBookmarkClient::CanBeEditedByUser(const BookmarkNode* node) {
  return !bookmarks::IsDescendantOf(node, managed_node_) &&
         !bookmarks::IsDescendantOf(node, supervised_node_);
}

void ChromeBookmarkClient::SetHistoryService(
    history::HistoryService* history_service) {
  DCHECK(history_service);
  history_service_ = history_service;
  favicon_changed_subscription_ = history_service_->AddFaviconChangedCallback(
      base::Bind(&BookmarkModel::OnFaviconChanged, base::Unretained(model_)));
}

void ChromeBookmarkClient::BookmarkModelChanged() {
}

void ChromeBookmarkClient::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  if (history_service_)
    history_service_->URLsNoLongerBookmarked(removed_urls);
}

void ChromeBookmarkClient::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  if (history_service_)
    history_service_->URLsNoLongerBookmarked(removed_urls);
}

void ChromeBookmarkClient::BookmarkModelLoaded(BookmarkModel* model,
                                               bool ids_reassigned) {
  // Start tracking the managed and supervised bookmarks. This will detect any
  // changes that may have occurred while the initial managed and supervised
  // bookmarks were being loaded on the background.
  managed_bookmarks_tracker_->Init(managed_node_);
  supervised_bookmarks_tracker_->Init(supervised_node_);
}

// static
bookmarks::BookmarkPermanentNodeList ChromeBookmarkClient::LoadExtraNodes(
    scoped_ptr<BookmarkPermanentNode> managed_node,
    scoped_ptr<base::ListValue> initial_managed_bookmarks,
    scoped_ptr<BookmarkPermanentNode> supervised_node,
    scoped_ptr<base::ListValue> initial_supervised_bookmarks,
    int64* next_node_id) {
  LoadInitialContents(
      managed_node.get(), initial_managed_bookmarks.get(), next_node_id);
  managed_node->SetTitle(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME));

  LoadInitialContents(
      supervised_node.get(), initial_supervised_bookmarks.get(), next_node_id);
  supervised_node->SetTitle(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BAR_SUPERVISED_FOLDER_DEFAULT_NAME));

  bookmarks::BookmarkPermanentNodeList extra_nodes;
  // Ownership of the managed and supervised nodes passed to the caller.
  extra_nodes.push_back(managed_node.release());
  extra_nodes.push_back(supervised_node.release());

  return extra_nodes.Pass();
}

std::string ChromeBookmarkClient::GetManagedBookmarksDomain() {
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile_);
  if (connector->IsPolicyFromCloudPolicy(policy::key::kManagedBookmarks))
    return connector->GetManagementDomain();
  return std::string();
}
