// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

class TokenServiceTableTest : public testing::Test {
 public:
  TokenServiceTableTest() {}
  virtual ~TokenServiceTableTest() {}

 protected:
  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db = "TestWebDatabase" +
        base::Int64ToString(Time::Now().ToTimeT()) +
        ".db";
    file_ = file_.AppendASCII(test_db);
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  FilePath file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TokenServiceTableTest);
};

TEST_F(TokenServiceTableTest, TokenServiceGetAllRemoveAll) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::map<std::string, std::string> out_map;
  std::string service;
  std::string service2;
  service = "testservice";
  service2 = "othertestservice";

  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that get all tokens works
  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service,
                                                            "pepperoni"));
  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service2, "steak"));
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "pepperoni");
  EXPECT_EQ(out_map.find(service2)->second, "steak");
  out_map.clear();

  // Purge
  EXPECT_TRUE(db.GetTokenServiceTable()->RemoveAllTokens());
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  // Check that you can still add it back in
  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service, "cheese"));
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "cheese");
}

TEST_F(TokenServiceTableTest, TokenServiceGetSet) {
  WebDatabase db;
  ASSERT_EQ(sql::INIT_OK, db.Init(file_));

  std::map<std::string, std::string> out_map;
  std::string service;
  service = "testservice";

  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_TRUE(out_map.empty());

  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service,
                                                            "pepperoni"));
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "pepperoni");
  out_map.clear();

  // try blanking it - won't remove it from the db though!
  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service, ""));
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "");
  out_map.clear();

  // try mutating it
  EXPECT_TRUE(db.GetTokenServiceTable()->SetTokenForService(service, "ham"));
  EXPECT_TRUE(db.GetTokenServiceTable()->GetAllTokens(&out_map));
  EXPECT_EQ(out_map.find(service)->second, "ham");
}
