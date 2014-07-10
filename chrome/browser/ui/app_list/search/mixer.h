// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/app_list/search/history_types.h"
#include "ui/app_list/app_list_model.h"

namespace app_list {

namespace test {
FORWARD_DECLARE_TEST(MixerTest, Publish);
}

class ChromeSearchResult;
class SearchProvider;

// Mixer collects results from providers, sorts them and publishes them to the
// SearchResults UI model. The targeted results have 6 slots to hold the
// result. These slots could be viewed as having three groups: main group
// (local apps and contacts), omnibox group and web store group. The
// main group takes no more than 4 slots. The web store takes no more than 2
// slots. The omnibox group takes all the remaining slots.
class Mixer {
 public:
  // The enum represents mixer groups. Note this must matches the order
  // of group creation in Init().
  enum GroupId {
    MAIN_GROUP = 0,
    OMNIBOX_GROUP = 1,
    WEBSTORE_GROUP = 2,
    PEOPLE_GROUP = 3,
  };

  explicit Mixer(AppListModel::SearchResults* ui_results);
  ~Mixer();

  // Creates mixer groups.
  void Init();

  // Associates a provider with a mixer group.
  void AddProviderToGroup(GroupId group, SearchProvider* provider);

  // Collects the results, sorts and publishes them.
  void MixAndPublish(const KnownResults& known_results);

 private:
  FRIEND_TEST_ALL_PREFIXES(test::MixerTest, Publish);

  // Used for sorting and mixing results.
  struct SortData {
    SortData();
    SortData(ChromeSearchResult* result, double score);

    bool operator<(const SortData& other) const;

    ChromeSearchResult* result;  // Not owned.
    double score;
  };
  typedef std::vector<Mixer::SortData> SortedResults;

  class Group;
  typedef ScopedVector<Group> Groups;

  // Publishes the given |new_results| to |ui_results|, deleting any existing
  // results that are not in |new_results|. Results that already exist in
  // |ui_results| are reused to avoid flickering caused by icon reload.
  static void Publish(const SortedResults& results,
                      AppListModel::SearchResults* ui_results);

  // Removes duplicates from |results|.
  static void RemoveDuplicates(SortedResults* results);

  void FetchResults(const KnownResults& known_results);

  AppListModel::SearchResults* ui_results_;  // Not owned.
  Groups groups_;

  DISALLOW_COPY_AND_ASSIGN(Mixer);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_MIXER_H_
