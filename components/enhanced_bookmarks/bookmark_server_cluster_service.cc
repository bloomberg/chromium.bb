// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/bookmark_server_cluster_service.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "components/enhanced_bookmarks/pref_names.h"
#include "components/enhanced_bookmarks/proto/cluster.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/base/url_util.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

using bookmarks::BookmarkNode;

namespace {
const char kClusterUrl[] = "https://www.google.com/stars/cluster";
const int kPrefServiceVersion = 1;
const char kPrefServiceVersionKey[] = "version";
const char kPrefServiceDataKey[] = "data";
const char kAuthIdKey[] = "auth_id";
}  // namespace

namespace enhanced_bookmarks {

BookmarkServerClusterService::BookmarkServerClusterService(
    const std::string& application_language_code,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    enhanced_bookmarks::EnhancedBookmarkModel* enhanced_bookmark_model,
    PrefService* pref_service)
    : BookmarkServerService(request_context_getter,
                            token_service,
                            signin_manager,
                            enhanced_bookmark_model),
      application_language_code_(application_language_code),
      pref_service_(pref_service) {
  LoadModel();

  if (model_->loaded())
    TriggerTokenRequest(false);

  GetSigninManager()->AddObserver(this);
}

BookmarkServerClusterService::~BookmarkServerClusterService() {
  GetSigninManager()->RemoveObserver(this);
}

const std::vector<const BookmarkNode*>
BookmarkServerClusterService::BookmarksForClusterNamed(
    const std::string& cluster_name) const {
  std::vector<const BookmarkNode*> results;

  ClusterMap::const_iterator cluster_it = cluster_data_.find(cluster_name);
  if (cluster_it == cluster_data_.end())
    return results;

  for (auto& star_id : cluster_it->second) {
    const BookmarkNode* bookmark = BookmarkForRemoteId(star_id);
    if (bookmark)
      results.push_back(bookmark);
  }
  return results;
}

const std::vector<std::string>
BookmarkServerClusterService::ClustersForBookmark(
    const BookmarkNode* bookmark) const {
  const std::string& star_id = RemoteIDForBookmark(bookmark);

  // TODO(noyau): if this turns out to be a perf bottleneck this may be improved
  // by storing a reverse map from id to cluster.
  std::vector<std::string> clusters;
  for (auto& pair : cluster_data_) {
    const std::vector<std::string>& stars_ids = pair.second;
    if (std::find(stars_ids.begin(), stars_ids.end(), star_id) !=
        stars_ids.end())
      clusters.push_back(pair.first);
  }
  return clusters;
}

const std::vector<std::string> BookmarkServerClusterService::GetClusters()
    const {
  std::vector<std::string> cluster_names;

  for (auto& pair : cluster_data_) {
    for (auto& star_id : pair.second) {
      const BookmarkNode* bookmark = BookmarkForRemoteId(star_id);
      if (bookmark) {
        // Only add clusters that have children.
        cluster_names.push_back(pair.first);
        break;
      }
    }
  }

  return cluster_names;
}

// static
void BookmarkServerClusterService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kBookmarkClusters,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

scoped_ptr<net::URLFetcher> BookmarkServerClusterService::CreateFetcher() {
  // Add the necessary arguments to the URI.
  GURL url(kClusterUrl);
  url = net::AppendQueryParameter(url, "output", "proto");

  // Append language.
  if (!application_language_code_.empty())
    url = net::AppendQueryParameter(url, "hl", application_language_code_);

  url = net::AppendQueryParameter(url, "v", model_->GetVersionString());

  // Build the URLFetcher to perform the request.
  scoped_ptr<net::URLFetcher> url_fetcher(
      net::URLFetcher::Create(url, net::URLFetcher::POST, this));

  // Binary encode a basic request proto.
  image_collections::ClusterRequest request_proto;
  request_proto.set_cluster_all(true);

  std::string proto_output;
  bool result = request_proto.SerializePartialToString(&proto_output);
  DCHECK(result);

  url_fetcher->SetUploadData("application/octet-stream", proto_output);
  return url_fetcher;
}

bool BookmarkServerClusterService::ProcessResponse(const std::string& response,
                                                   bool* should_notify) {
  DCHECK(*should_notify);
  image_collections::ClusterResponse response_proto;
  bool result = response_proto.ParseFromString(response);
  if (!result)
    return false;  // Not formatted properly.

  ClusterMap new_cluster_data;
  for (const auto& cluster : response_proto.clusters()) {
    const std::string& title = cluster.title();
    if (title.empty())
      continue;
    std::vector<std::string> stars_ids;
    for (auto& doc : cluster.docs()) {
      if (!doc.empty())
        stars_ids.push_back(doc);
    }
    if (stars_ids.size())
      new_cluster_data[title] = stars_ids;
  }

  if (new_cluster_data.size() == cluster_data_.size() &&
      std::equal(new_cluster_data.begin(),
                 new_cluster_data.end(),
                 cluster_data_.begin())) {
    *should_notify = false;
  } else {
    SwapModel(&new_cluster_data);
  }
  return true;
}

void BookmarkServerClusterService::CleanAfterFailure() {
  if (cluster_data_.empty())
    return;

  ClusterMap empty;
  SwapModel(&empty);
}

void BookmarkServerClusterService::EnhancedBookmarkModelLoaded() {
  TriggerTokenRequest(false);
}

void BookmarkServerClusterService::EnhancedBookmarkAdded(
    const BookmarkNode* node) {
  // Nothing to do.
}

void BookmarkServerClusterService::EnhancedBookmarkRemoved(
    const BookmarkNode* node) {
  // It is possible to remove the entries from the map here, but as those are
  // filtered in ClustersForBookmark() this is not strictly necessary.
}

void BookmarkServerClusterService::EnhancedBookmarkNodeChanged(
    const BookmarkNode* node) {
  // Nothing to do.
}

void BookmarkServerClusterService::EnhancedBookmarkAllUserNodesRemoved() {
  if (!cluster_data_.empty()) {
    ClusterMap empty;
    SwapModel(&empty);
  }
}

void BookmarkServerClusterService::EnhancedBookmarkRemoteIdChanged(
    const BookmarkNode* node,
    const std::string& old_remote_id,
    const std::string& remote_id) {
  std::vector<std::string> clusters;
  for (auto& pair : cluster_data_) {
    std::vector<std::string>& stars_ids = pair.second;
    std::replace(stars_ids.begin(), stars_ids.end(), old_remote_id, remote_id);
  }
}

void BookmarkServerClusterService::GoogleSignedOut(
    const std::string& account_id,
    const std::string& username) {
  if (!cluster_data_.empty()) {
    ClusterMap empty;
    SwapModel(&empty);
  }
}

void BookmarkServerClusterService::SwapModel(ClusterMap* cluster_map) {
  cluster_data_.swap(*cluster_map);
  const std::string& auth_id = GetSigninManager()->GetAuthenticatedAccountId();
  scoped_ptr<base::DictionaryValue> dictionary(
      Serialize(cluster_data_, auth_id));
  pref_service_->Set(prefs::kBookmarkClusters, *dictionary);
}

void BookmarkServerClusterService::LoadModel() {
  const base::DictionaryValue* dictionary =
      pref_service_->GetDictionary(prefs::kBookmarkClusters);
  const std::string& auth_id = GetSigninManager()->GetAuthenticatedAccountId();

  ClusterMap loaded_data;
  bool result = BookmarkServerClusterService::Deserialize(
      *dictionary, auth_id, &loaded_data);
  if (result)
    cluster_data_.swap(loaded_data);
}

//
// Serialization.
//
// static
scoped_ptr<base::DictionaryValue> BookmarkServerClusterService::Serialize(
    const ClusterMap& cluster_map,
    const std::string& auth_id) {
  // Create a list of all clusters. For each cluster, make another list. The
  // first element in the list is the key (cluster name). All subsequent
  // elements are stars ids.
  scoped_ptr<base::ListValue> all_clusters(new base::ListValue);
  for (auto& pair : cluster_map) {
    scoped_ptr<base::ListValue> cluster(new base::ListValue);
    cluster->AppendString(pair.first);
    cluster->AppendStrings(pair.second);
    all_clusters->Append(cluster.release());
  }

  // The dictionary that will be serialized has two fields: a version field and
  // a data field.
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue);
  data->SetInteger(kPrefServiceVersionKey, kPrefServiceVersion);
  data->Set(kPrefServiceDataKey, all_clusters.release());
  data->SetString(kAuthIdKey, auth_id);

  return data.Pass();
}

// static
bool BookmarkServerClusterService::Deserialize(
    const base::DictionaryValue& value,
    const std::string& auth_id,
    ClusterMap* out_map) {
  ClusterMap output;

  // Check version.
  int version;
  if (!value.GetInteger(kPrefServiceVersionKey, &version))
    return false;
  if (version != kPrefServiceVersion)
    return false;

  // Check auth id.
  std::string id;
  if (!value.GetString(kAuthIdKey, &id))
    return false;
  if (id != auth_id)
    return false;

  const base::ListValue* all_clusters = NULL;
  if (!value.GetList(kPrefServiceDataKey, &all_clusters))
    return false;

  for (size_t index = 0; index < all_clusters->GetSize(); ++index) {
    const base::ListValue* cluster = NULL;
    if (!all_clusters->GetList(index, &cluster))
      return false;
    if (cluster->GetSize() < 1)
      return false;
    std::string key;
    if (!cluster->GetString(0, &key))
      return false;
    std::vector<std::string> stars_ids;
    for (size_t index = 1; index < cluster->GetSize(); ++index) {
      std::string stars_id;
      if (!cluster->GetString(index, &stars_id))
        return false;
      stars_ids.push_back(stars_id);
    }
    output.insert(std::make_pair(key, stars_ids));
  }
  out_map->swap(output);
  return true;
}

}  // namespace enhanced_bookmarks
