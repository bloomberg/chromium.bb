// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_changed_details.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "chrome/browser/profiles/startup_task_runner_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void NotifyHistoryOfRemovedURLs(Profile* profile,
                                const std::set<GURL>& removed_urls) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->URLsNoLongerBookmarked(removed_urls);
}

}  // namespace

ChromeBookmarkClient::ChromeBookmarkClient(Profile* profile)
    : profile_(profile), model_(NULL), managed_node_(NULL) {
}

ChromeBookmarkClient::~ChromeBookmarkClient() {
}

void ChromeBookmarkClient::Init(BookmarkModel* model) {
  DCHECK(model);
  DCHECK(!model_);
  model_ = model;
  model_->AddObserver(this);

  managed_bookmarks_tracker_.reset(new policy::ManagedBookmarksTracker(
      model_,
      profile_->GetPrefs(),
      base::Bind(&ChromeBookmarkClient::GetManagedBookmarksDomain,
                 base::Unretained(this))));

  // Listen for changes to favicons so that we can update the favicon of the
  // node appropriately.
  registrar_.Add(this,
                 chrome::NOTIFICATION_FAVICON_CHANGED,
                 content::Source<Profile>(profile_));
}

void ChromeBookmarkClient::Shutdown() {
  if (model_) {
    registrar_.RemoveAll();

    model_->RemoveObserver(this);
    model_ = NULL;
  }
  BookmarkClient::Shutdown();
}

bool ChromeBookmarkClient::IsDescendantOfManagedNode(const BookmarkNode* node) {
  return node && node->HasAncestor(managed_node_);
}

bool ChromeBookmarkClient::HasDescendantsOfManagedNode(
    const std::vector<const BookmarkNode*>& list) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (IsDescendantOfManagedNode(list[i]))
      return true;
  }
  return false;
}

bool ChromeBookmarkClient::PreferTouchIcon() {
#if !defined(OS_IOS)
  return false;
#else
  return true;
#endif
}

base::CancelableTaskTracker::TaskId ChromeBookmarkClient::GetFaviconImageForURL(
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const favicon_base::FaviconImageCallback& callback,
    base::CancelableTaskTracker* tracker) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return base::CancelableTaskTracker::kBadTaskId;
  return favicon_service->GetFaviconImageForPageURL(
      FaviconService::FaviconForPageURLParams(
          page_url, icon_types, desired_size_in_dip),
      callback,
      tracker);
}

bool ChromeBookmarkClient::SupportsTypedCountForNodes() {
  return true;
}

void ChromeBookmarkClient::GetTypedCountForNodes(
    const NodeSet& nodes,
    NodeTypedCountPairs* node_typed_count_pairs) {
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : NULL;
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
         node == managed_node_);
  if (node == managed_node_)
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
  // Create the managed_node now; it will be populated in the LoadExtraNodes
  // callback.
  // The ownership of managed_node_ is in limbo until LoadExtraNodes runs,
  // so we leave it in the care of the closure meanwhile.
  scoped_ptr<BookmarkPermanentNode> managed(new BookmarkPermanentNode(0));
  managed_node_ = managed.get();

  return base::Bind(
      &ChromeBookmarkClient::LoadExtraNodes,
      StartupTaskRunnerServiceFactory::GetForProfile(profile_)
          ->GetBookmarkTaskRunner(),
      base::Passed(&managed),
      base::Passed(managed_bookmarks_tracker_->GetInitialManagedBookmarks()));
}

bool ChromeBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  // The |managed_node_| can have its title updated if the user signs in or
  // out.
  return !IsDescendantOfManagedNode(permanent_node) ||
         permanent_node == managed_node_;
}

bool ChromeBookmarkClient::CanSyncNode(const BookmarkNode* node) {
  return !IsDescendantOfManagedNode(node);
}

bool ChromeBookmarkClient::CanBeEditedByUser(const BookmarkNode* node) {
  return !IsDescendantOfManagedNode(node);
}

void ChromeBookmarkClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_FAVICON_CHANGED: {
      content::Details<FaviconChangedDetails> favicon_details(details);
      model_->OnFaviconChanged(favicon_details->urls);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void ChromeBookmarkClient::BookmarkModelChanged() {
}

void ChromeBookmarkClient::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  NotifyHistoryOfRemovedURLs(profile_, removed_urls);
}

void ChromeBookmarkClient::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  NotifyHistoryOfRemovedURLs(profile_, removed_urls);
}

void ChromeBookmarkClient::BookmarkModelLoaded(BookmarkModel* model,
                                               bool ids_reassigned) {
  // Start tracking the managed bookmarks. This will detect any changes that
  // may have occurred while the initial managed bookmarks were being loaded
  // on the background.
  managed_bookmarks_tracker_->Init(managed_node_);
}

// static
bookmarks::BookmarkPermanentNodeList ChromeBookmarkClient::LoadExtraNodes(
    const scoped_refptr<base::DeferredSequencedTaskRunner>& profile_io_runner,
    scoped_ptr<BookmarkPermanentNode> managed_node,
    scoped_ptr<base::ListValue> initial_managed_bookmarks,
    int64* next_node_id) {
  DCHECK(profile_io_runner->RunsTasksOnCurrentThread());
  // Load the initial contents of the |managed_node| now, and assign it an
  // unused ID.
  int64 managed_id = *next_node_id;
  managed_node->set_id(managed_id);
  *next_node_id = policy::ManagedBookmarksTracker::LoadInitial(
      managed_node.get(), initial_managed_bookmarks.get(), managed_id + 1);
  managed_node->set_visible(!managed_node->empty());
  managed_node->SetTitle(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME));

  bookmarks::BookmarkPermanentNodeList extra_nodes;
  // Ownership of the managed node passed to the caller.
  extra_nodes.push_back(managed_node.release());

  return extra_nodes.Pass();
}

std::string ChromeBookmarkClient::GetManagedBookmarksDomain() {
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile_);
  if (connector->IsPolicyFromCloudPolicy(policy::key::kManagedBookmarks))
    return connector->GetManagementDomain();
  return std::string();
}
