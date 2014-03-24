// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

class TokenServiceTableTest : public testing::Test {
 public:
  TokenServiceTableTest() {}
  virtual ~TokenServiceTableTest() {}

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.path().AppendASCII("TestWebDatabase");

    table_.reset(new TokenServiceTable);
    db_.reset(new WebDatabase);
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TokenServiceTable> table_;
  scoped_ptr<WebDatabase> db_;
 private:
  DISALLOW_COPY_AND_ASSIGN(TokenServiceTableTest);
};

// Flaky on mac_rel. See http://crbug.com/228943
#if defined(OS_MACOSX)
#define MAYBE_TokenServiceGetAllRemoveAll DISABLED_TokenServiceGetAllRemoveAll
#define MAYBE_TokenServiceGetSet DISABLED_TokenServiceGetSet
#define MAYBE_TokenServiceRemove DISABLED_TokenServiceRemove
#else
#define MAYBE_TokenServiceGetAllRemoveAll TokenServiceGetAllRemoveAll
#define MAYBE_TokenServiceGetSet TokenServiceGetSet
#define MAYBE_TokenServiceRemove TokenServiceRemove
#endif

TEST_F(TokenServiceTableTest, MAYBE_TokenServiceGetAllRemoveAll) {
  std::map<std::string, std::string> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that get all tokens works
  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni"));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak"));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ("pepperoni", out_map.find(service)->second);
  EXPECT_EQ("steak", out_map.find(service2)->second);
  out_map.clear();

  // Purge
  EXPECT_TRUE(table_->RemoveAllTokens());
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that you can still add it back in
  EXPECT_TRUE(table_->SetTokenForService(service, "cheese"));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ("cheese", out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, MAYBE_TokenServiceGetSet) {
  std::map<std::string, std::string> out_map;
  std::string service;
  service = "testservice";

  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni"));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ("pepperoni", out_map.find(service)->second);
  out_map.clear();

  // try blanking it - won't remove it from the db though!
  EXPECT_TRUE(table_->SetTokenForService(service, std::string()));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ("", out_map.find(service)->second);
  out_map.clear();

  // try mutating it
  EXPECT_TRUE(table_->SetTokenForService(service, "ham"));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ("ham", out_map.find(service)->second);
}

TEST_F(TokenServiceTableTest, MAYBE_TokenServiceRemove) {
  std::map<std::string, std::string> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(table_->SetTokenForService(service, "pepperoni"));
  EXPECT_TRUE(table_->SetTokenForService(service2, "steak"));
  EXPECT_TRUE(table_->RemoveTokenForService(service));
  EXPECT_TRUE(table_->GetAllTokens(&out_map));
  EXPECT_EQ(0u, out_map.count(service));
  EXPECT_EQ("steak", out_map.find(service2)->second);
}
