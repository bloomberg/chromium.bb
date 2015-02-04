// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "components/enhanced_bookmarks/bookmark_server_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "net/url_request/url_fetcher.h"

class PrefService;

namespace enhanced_bookmarks {

// Manages requests to the bookmark server to retrieve the current clustering
// state for the bookmarks. A cluster is simply a named set of bookmarks related
// to each others.
class BookmarkServerClusterService : public KeyedService,
                                     public BookmarkServerService,
                                     public SigninManagerBase::Observer {
 public:
  // Maps a cluster name to the stars.id of the bookmarks.
  typedef std::map<std::string, std::vector<std::string>> ClusterMap;
  // |application_language_code| should be a ISO 639-1 compliant string. Aka
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (english language with US locale) and 'en-GB_US'
  // (British english person in the US) are not language code.
  BookmarkServerClusterService(
      const std::string& application_language_code,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      EnhancedBookmarkModel* enhanced_bookmark_model,
      PrefService* pref_service);
  ~BookmarkServerClusterService() override;

  // Retrieves all the bookmarks associated with a cluster. The returned
  // BookmarkNodes are owned by the bookmark model, and one must listen to the
  // model observer notification to clear them.
  const std::vector<const bookmarks::BookmarkNode*> BookmarksForClusterNamed(
      const std::string& cluster_name) const;

  // Returns the clusters in which the passed bookmark is in, if any.
  const std::vector<std::string> ClustersForBookmark(
      const bookmarks::BookmarkNode* bookmark) const;

  // Dynamically generates a vector of all clusters names.
  const std::vector<std::string> GetClusters() const;

  // Registers server cluster service prefs.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  // BookmarkServerService methods.
  scoped_ptr<net::URLFetcher> CreateFetcher() override;
  bool ProcessResponse(const std::string& response,
                       bool* should_notify) override;
  void CleanAfterFailure() override;

  // EnhancedBookmarkModelObserver methods.
  void EnhancedBookmarkModelLoaded() override;
  void EnhancedBookmarkAdded(const bookmarks::BookmarkNode* node) override;
  void EnhancedBookmarkRemoved(const bookmarks::BookmarkNode* node) override;
  void EnhancedBookmarkNodeChanged(
      const bookmarks::BookmarkNode* node) override;
  void EnhancedBookmarkAllUserNodesRemoved() override;
  void EnhancedBookmarkRemoteIdChanged(const bookmarks::BookmarkNode* node,
                                       const std::string& old_remote_id,
                                       const std::string& remote_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, Cluster);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, SignOut);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, Serialization);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, SaveToPrefs);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, BadAuth);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, EmptyAuth);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest,
                           ClearClusterMapOnRemoveAllBookmarks);

  // Overriden from SigninManagerBase::Observer.
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // Updates |cluster_data_| with the |cluster_map| and saves the result to
  // profile prefs. All changes to |cluster_data_| should go through this method
  // to ensure profile prefs is always up to date.
  // TODO(noyau): This is probably a misuse of profile prefs. While the expected
  // amount of data is small (<1kb), it can theoretically reach megabytes in
  // size.
  void SwapModel(ClusterMap* cluster_map);
  // Updates |cluster_data_| from profile prefs.
  void LoadModel();

  // Serialize the |cluster_map| into the returned dictionary value.. The
  // |auth_id| uniquely identify the signed in user, to avoid deserializing data
  // for a different one.
  static scoped_ptr<base::DictionaryValue> Serialize(
      const ClusterMap& cluster_map,
      const std::string& auth_id);
  // Returns true on success.
  // The result is swapped into |out_map|.
  // |auth_id| must match the serialized auth_id for this method to succeed.
  static bool Deserialize(const base::DictionaryValue& value,
                          const std::string& auth_id,
                          ClusterMap* out_map);

  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;
  // The preferences services associated with the relevant profile.
  PrefService* pref_service_;
  // The cluster data, a map from cluster name to a vector of stars.id.
  ClusterMap cluster_data_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkServerClusterService);
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_CLUSTER_SERVICE_H_
