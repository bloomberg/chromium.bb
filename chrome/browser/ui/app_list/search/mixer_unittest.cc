// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/history_types.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_provider.h"

namespace app_list {
namespace test {

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance)
      : instance_id_(instantiation_count++) {
    set_id(id);
    set_title(base::UTF8ToUTF16(id));
    set_relevance(relevance);
  }
  virtual ~TestSearchResult() {}

  // ChromeSearchResult overides:
  virtual void Open(int event_flags) OVERRIDE {}
  virtual void InvokeAction(int action_index, int event_flags) OVERRIDE {}
  virtual scoped_ptr<ChromeSearchResult> Duplicate() OVERRIDE {
    return scoped_ptr<ChromeSearchResult>(
        new TestSearchResult(id(), relevance())).Pass();
  }
  virtual ChromeSearchResultType GetType() OVERRIDE {
    return SEARCH_RESULT_TYPE_BOUNDARY;
  }

  // For reference equality testing. (Addresses cannot be used to test reference
  // equality because it is possible that an object will be allocated at the
  // same address as a previously deleted one.)
  static int GetInstanceId(SearchResult* result) {
    return static_cast<const TestSearchResult*>(result)->instance_id_;
  }

 private:
  static int instantiation_count;

  int instance_id_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};
int TestSearchResult::instantiation_count = 0;

class TestSearchProvider : public SearchProvider {
 public:
  explicit TestSearchProvider(const std::string& prefix)
      : prefix_(prefix),
        count_(0) {}
  virtual ~TestSearchProvider() {}

  // SearchProvider overrides:
  virtual void Start(const base::string16& query) OVERRIDE {
    ClearResults();
    for (size_t i = 0; i < count_; ++i) {
      const std::string id =
          base::StringPrintf("%s%d", prefix_.c_str(), static_cast<int>(i));
      const double relevance = 1.0 - i / 10.0;
      Add(scoped_ptr<SearchResult>(new TestSearchResult(id, relevance)).Pass());
    }
  }
  virtual void Stop() OVERRIDE {}

  void set_prefix(const std::string& prefix) { prefix_ = prefix; }
  void set_count(size_t count) { count_ = count; }

 private:
  std::string prefix_;
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(TestSearchProvider);
};

class MixerTest : public testing::Test {
 public:
  MixerTest() {}
  virtual ~MixerTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    results_.reset(new AppListModel::SearchResults);

    providers_.push_back(new TestSearchProvider("app"));
    providers_.push_back(new TestSearchProvider("omnibox"));
    providers_.push_back(new TestSearchProvider("webstore"));
    providers_.push_back(new TestSearchProvider("people"));

    mixer_.reset(new Mixer(results_.get()));
    mixer_->Init();
    mixer_->AddProviderToGroup(Mixer::MAIN_GROUP, providers_[0]);
    mixer_->AddProviderToGroup(Mixer::OMNIBOX_GROUP, providers_[1]);
    mixer_->AddProviderToGroup(Mixer::WEBSTORE_GROUP, providers_[2]);
    mixer_->AddProviderToGroup(Mixer::PEOPLE_GROUP, providers_[3]);
  }

  void RunQuery() {
    const base::string16 query;

    for (size_t i = 0; i < providers_.size(); ++i) {
      providers_[i]->Start(query);
      providers_[i]->Stop();
    }

    mixer_->MixAndPublish(KnownResults());
  }

  std::string GetResults() const {
    std::string result;
    for (size_t i = 0; i < results_->item_count(); ++i) {
      if (!result.empty())
        result += ',';

      result += base::UTF16ToUTF8(results_->GetItemAt(i)->title());
    }

    return result;
  }

  Mixer* mixer() { return mixer_.get(); }
  TestSearchProvider* app_provider() { return providers_[0]; }
  TestSearchProvider* omnibox_provider() { return providers_[1]; }
  TestSearchProvider* webstore_provider() { return providers_[2]; }

 private:
  scoped_ptr<Mixer> mixer_;
  scoped_ptr<AppListModel::SearchResults> results_;

  ScopedVector<TestSearchProvider> providers_;

  DISALLOW_COPY_AND_ASSIGN(MixerTest);
};

TEST_F(MixerTest, Basic) {
  struct TestCase {
    const size_t app_results;
    const size_t omnibox_results;
    const size_t webstore_results;
    const char* expected;
  } kTestCases[] = {
    {0, 0, 0, ""},
    {4, 6, 2, "app0,app1,app2,app3,omnibox0,webstore0"},
    {10, 10, 10, "app0,app1,app2,app3,omnibox0,webstore0"},
    {0, 10, 0, "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,omnibox5"},
    {0, 10, 1, "omnibox0,omnibox1,omnibox2,omnibox3,omnibox4,webstore0"},
    {0, 10, 2, "omnibox0,omnibox1,omnibox2,omnibox3,webstore0,webstore1"},
    {1, 10, 0, "app0,omnibox0,omnibox1,omnibox2,omnibox3,omnibox4"},
    {2, 10, 0, "app0,app1,omnibox0,omnibox1,omnibox2,omnibox3"},
    {2, 10, 1, "app0,app1,omnibox0,omnibox1,omnibox2,webstore0"},
    {2, 10, 2, "app0,app1,omnibox0,omnibox1,webstore0,webstore1"},
    {2, 0, 2, "app0,app1,webstore0,webstore1"},
    {0, 0, 0, ""},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    app_provider()->set_count(kTestCases[i].app_results);
    omnibox_provider()->set_count(kTestCases[i].omnibox_results);
    webstore_provider()->set_count(kTestCases[i].webstore_results);
    RunQuery();

    EXPECT_EQ(kTestCases[i].expected, GetResults()) << "Case " << i;
  }
}

TEST_F(MixerTest, RemoveDuplicates) {
  const std::string dup = "dup";

  // This gives "dup0,dup1,dup2".
  app_provider()->set_prefix(dup);
  app_provider()->set_count(3);

  // This gives "dup0,dup1".
  omnibox_provider()->set_prefix(dup);
  omnibox_provider()->set_count(2);

  // This gives "dup0".
  webstore_provider()->set_prefix(dup);
  webstore_provider()->set_count(1);

  RunQuery();

  // Only three results with unique id are kept.
  EXPECT_EQ("dup0,dup1,dup2", GetResults());
}

TEST_F(MixerTest, Publish) {
  scoped_ptr<ChromeSearchResult> result1(new TestSearchResult("app1", 0));
  scoped_ptr<ChromeSearchResult> result2(new TestSearchResult("app2", 0));
  scoped_ptr<ChromeSearchResult> result3(new TestSearchResult("app3", 0));
  scoped_ptr<ChromeSearchResult> result3_copy = result3->Duplicate();
  scoped_ptr<ChromeSearchResult> result4(new TestSearchResult("app4", 0));
  scoped_ptr<ChromeSearchResult> result5(new TestSearchResult("app5", 0));

  AppListModel::SearchResults ui_results;

  // Publish the first three results to |ui_results|.
  Mixer::SortedResults new_results;
  new_results.push_back(Mixer::SortData(result1.get(), 1.0f));
  new_results.push_back(Mixer::SortData(result2.get(), 1.0f));
  new_results.push_back(Mixer::SortData(result3.get(), 1.0f));

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(3u, ui_results.item_count());
  // The objects in |ui_results| should be new copies because the input results
  // are owned and |ui_results| needs to own its results as well.
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[0].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[1].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(1)));
  EXPECT_NE(TestSearchResult::GetInstanceId(new_results[2].result),
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));

  // Save the current |ui_results| instance ids for comparison later.
  std::vector<int> old_ui_result_ids;
  for (size_t i = 0; i < ui_results.item_count(); ++i) {
    old_ui_result_ids.push_back(
        TestSearchResult::GetInstanceId(ui_results.GetItemAt(i)));
  }

  // Change the first result to a totally new object (with a new ID).
  new_results[0] = Mixer::SortData(result4.get(), 1.0f);

  // Change the second result's title, but keep the same id. (The result will
  // keep the id "app2" but change its title to "New App 2 Title".)
  const base::string16 kNewAppTitle = base::UTF8ToUTF16("New App 2 Title");
  new_results[1].result->set_title(kNewAppTitle);

  // Change the third result's object address (it points to an object with the
  // same data).
  new_results[2] = Mixer::SortData(result3_copy.get(), 1.0f);

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(3u, ui_results.item_count());

  // The first result will be a new object, as the ID has changed.
  EXPECT_NE(old_ui_result_ids[0],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));

  // The second result will still use the original object, but have a different
  // title, since the ID did not change.
  EXPECT_EQ(old_ui_result_ids[1],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(1)));
  EXPECT_EQ(kNewAppTitle, ui_results.GetItemAt(1)->title());

  // The third result will use the original object as the ID did not change.
  EXPECT_EQ(old_ui_result_ids[2],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));

  // Save the current |ui_results| order which should is app4, app2, app3.
  old_ui_result_ids.clear();
  for (size_t i = 0; i < ui_results.item_count(); ++i) {
    old_ui_result_ids.push_back(
        TestSearchResult::GetInstanceId(ui_results.GetItemAt(i)));
  }

  // Reorder the existing results and add a new one in the second place.
  new_results[0] = Mixer::SortData(result2.get(), 1.0f);
  new_results[1] = Mixer::SortData(result5.get(), 1.0f);
  new_results[2] = Mixer::SortData(result3.get(), 1.0f);
  new_results.push_back(Mixer::SortData(result4.get(), 1.0f));

  Mixer::Publish(new_results, &ui_results);
  EXPECT_EQ(4u, ui_results.item_count());

  // The reordered results should use the original objects.
  EXPECT_EQ(old_ui_result_ids[0],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(3)));
  EXPECT_EQ(old_ui_result_ids[1],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(0)));
  EXPECT_EQ(old_ui_result_ids[2],
            TestSearchResult::GetInstanceId(ui_results.GetItemAt(2)));
}

}  // namespace test
}  // namespace app_list
