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
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_model.h"

namespace app_list {
namespace test {

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id, double relevance) {
    set_id(id);
    set_title(UTF8ToUTF16(id));
    set_relevance(relevance);
  }
  virtual ~TestSearchResult() {}

 private:
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

  DISALLOW_COPY_AND_ASSIGN(TestSearchResult);
};

class TestSearchProvider : public SearchProvider {
 public:
  explicit TestSearchProvider(const std::string& prefix)
      : prefix_(prefix),
        count_(0) {}
  virtual ~TestSearchProvider() {}

  // SearchProvider overrides:
  virtual void Start(const string16& query) OVERRIDE {
    ClearResults();
    for (size_t i = 0; i < count_; ++i) {
      const std::string id =
          base::StringPrintf("%s%d", prefix_.c_str(), static_cast<int>(i));
      const double relevance = 1.0 - i / 10.0;
      Add(scoped_ptr<ChromeSearchResult>(
          new TestSearchResult(id, relevance)).Pass());
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
    const string16 query;

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

      result += UTF16ToUTF8(results_->GetItemAt(i)->title());
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

}  // namespace test
}  // namespace app_list
