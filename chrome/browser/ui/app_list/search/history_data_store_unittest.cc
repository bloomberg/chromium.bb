// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/app_list/search/history_data.h"
#include "chrome/browser/ui/app_list/search/history_data_store.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

namespace {

std::string GetDataContent(const HistoryData::Data& data) {
  std::string str = std::string("p:") + data.primary + ";s:";
  bool first = true;
  for (HistoryData::SecondaryDeque::const_iterator it = data.secondary.begin();
       it != data.secondary.end(); ++it) {
    if (first)
      first = false;
    else
      str += ',';

    str += *it;
  }

  return str;
}

}  // namespace

class HistoryDataStoreTest : public testing::Test {
 public:
  HistoryDataStoreTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {}
  virtual ~HistoryDataStoreTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  virtual void TearDown() OVERRIDE {
    // Release |store_| while ui loop is still running.
    store_ = NULL;
  }

  void OpenStore(const std::string& file_name) {
    data_file_ = temp_dir_.path().AppendASCII(file_name);
    store_ = new HistoryDataStore(data_file_);
    Load();
  }

  void Flush() {
    store_->Flush(DictionaryDataStore::OnFlushedCallback());
  }

  void Load() {
    store_->Load(base::Bind(&HistoryDataStoreTest::OnRead,
                            base::Unretained(this)));
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

  void WriteDataFile(const std::string& file_name,
                     const std::string& data) {
    base::WriteFile(
        temp_dir_.path().AppendASCII(file_name), data.c_str(), data.size());
  }

  HistoryDataStore* store() { return store_.get(); }
  const HistoryData::Associations& associations() const {
    return associations_;
  }

 private:
  void OnRead(scoped_ptr<HistoryData::Associations> associations) {
    associations_.clear();
    if (associations)
      associations->swap(associations_);

    if (run_loop_)
      run_loop_->Quit();
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
  base::FilePath data_file_;
  scoped_ptr<base::RunLoop> run_loop_;

  scoped_refptr<HistoryDataStore> store_;
  HistoryData::Associations associations_;

  DISALLOW_COPY_AND_ASSIGN(HistoryDataStoreTest);
};

TEST_F(HistoryDataStoreTest, NewFile) {
  OpenStore("new_data_file.json");
  EXPECT_TRUE(associations().empty());
}

TEST_F(HistoryDataStoreTest, BadFile) {
  const char kDataFile[] = "invalid_data_file";
  WriteDataFile(kDataFile, "invalid json");

  OpenStore(kDataFile);
  EXPECT_TRUE(associations().empty());
}

TEST_F(HistoryDataStoreTest, GoodFile) {
  const char kDataFile[] = "good_data_file.json";
  const char kGoodJson[] = "{"
      "\"version\": \"1\","
      "\"associations\": {"
          "\"query\": {"
             "\"p\": \"primary\","
             "\"s\": [\"secondary1\",\"secondary2\"],"
             "\"t\": \"123\""
          "}"
        "}"
      "}";
  WriteDataFile(kDataFile, kGoodJson);

  OpenStore(kDataFile);
  EXPECT_FALSE(associations().empty());
  EXPECT_EQ(1u, associations().size());

  HistoryData::Associations::const_iterator it = associations().find("query");
  EXPECT_TRUE(it != associations().end());
  EXPECT_EQ("p:primary;s:secondary1,secondary2", GetDataContent(it->second));
}

TEST_F(HistoryDataStoreTest, Change) {
  const char kDataFile[] = "change_test.json";

  OpenStore(kDataFile);
  EXPECT_TRUE(associations().empty());

  const char kQuery[] = "query";
  const base::Time now = base::Time::Now();
  store()->SetPrimary(kQuery, "primary");
  store()->SetUpdateTime(kQuery, now);
  Flush();
  Load();
  EXPECT_EQ(1u, associations().size());
  HistoryData::Associations::const_iterator it = associations().find(kQuery);
  EXPECT_TRUE(it != associations().end());
  EXPECT_EQ("primary", it->second.primary);
  EXPECT_EQ(0u, it->second.secondary.size());
  EXPECT_EQ(now, it->second.update_time);

  HistoryData::SecondaryDeque secondary;
  secondary.push_back("s1");
  secondary.push_back("s2");
  store()->SetSecondary(kQuery, secondary);
  Flush();
  Load();
  EXPECT_EQ(1u, associations().size());
  it = associations().find(kQuery);
  EXPECT_TRUE(it != associations().end());
  EXPECT_EQ("p:primary;s:s1,s2", GetDataContent(it->second));
  EXPECT_EQ(now, it->second.update_time);

  store()->Delete(kQuery);
  Flush();
  Load();
  EXPECT_TRUE(associations().empty());
}

}  // namespace test
}  // namespace app_list
