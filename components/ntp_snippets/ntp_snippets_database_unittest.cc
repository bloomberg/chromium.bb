// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_database.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;
using testing::_;

namespace ntp_snippets {

bool operator==(const SnippetSource& lhs, const SnippetSource& rhs) {
  return lhs.url == rhs.url && lhs.publisher_name == rhs.publisher_name &&
         lhs.amp_url == rhs.amp_url;
}

bool operator==(const NTPSnippet& lhs, const NTPSnippet& rhs) {
  return lhs.id() == rhs.id() && lhs.title() == rhs.title() &&
         lhs.snippet() == rhs.snippet() &&
         lhs.salient_image_url() == rhs.salient_image_url() &&
         lhs.publish_date() == rhs.publish_date() &&
         lhs.expiry_date() == rhs.expiry_date() &&
         lhs.source_index() == rhs.source_index() &&
         lhs.sources() == rhs.sources() && lhs.score() == rhs.score() &&
         lhs.is_discarded() == rhs.is_discarded();
}

namespace {

std::unique_ptr<NTPSnippet> CreateTestSnippet() {
  std::unique_ptr<NTPSnippet> snippet(new NTPSnippet("http://localhost"));
  snippet->add_source(
      SnippetSource(GURL("http://localhost"), "Publisher", GURL("http://amp")));
  return snippet;
}

MATCHER_P(SnippetEq, snippet, "") {
  return *arg == *snippet;
}

}  // namespace

class NTPSnippetsDatabaseTest : public testing::Test {
 public:
  NTPSnippetsDatabaseTest() {
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~NTPSnippetsDatabaseTest() override {
    // We need to run the message loop after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    db_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void CreateDatabase() {
    // Explicitly destroy any existing database first, so it releases the lock
    // on the file.
    db_.reset();

    db_.reset(new NTPSnippetsDatabase(database_dir_.path(),
                                      base::ThreadTaskRunnerHandle::Get()));
  }

  NTPSnippetsDatabase* db() { return db_.get(); }

  bool db_inited() { return db_->database_initialized_; }

  void OnSnippetsLoaded(NTPSnippet::PtrVector snippets) {
    OnSnippetsLoadedImpl(snippets);
  }

  MOCK_METHOD1(OnSnippetsLoadedImpl,
               void(const NTPSnippet::PtrVector& snippets));

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir database_dir_;
  std::unique_ptr<NTPSnippetsDatabase> db_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsDatabaseTest);
};

TEST_F(NTPSnippetsDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase();
  EXPECT_FALSE(db_inited());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db_inited());
}

TEST_F(NTPSnippetsDatabaseTest, LoadBeforeInit) {
  CreateDatabase();
  EXPECT_FALSE(db_inited());

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db_inited());
}

TEST_F(NTPSnippetsDatabaseTest, LoadAfterInit) {
  CreateDatabase();
  EXPECT_FALSE(db_inited());

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_)).Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(db_inited());

  Mock::VerifyAndClearExpectations(this);

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsDatabaseTest, Save) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db_inited());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();

  db()->Save(*snippet);
  base::RunLoop().RunUntilIdle();

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // The snippet should still exist after recreating the database.
  CreateDatabase();

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsDatabaseTest, Update) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db_inited());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();

  db()->Save(*snippet);
  base::RunLoop().RunUntilIdle();

  const std::string text("some text");
  snippet->set_snippet(text);

  db()->Save(*snippet);
  base::RunLoop().RunUntilIdle();

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(SnippetEq(snippet.get()))));
  base::RunLoop().RunUntilIdle();
}

TEST_F(NTPSnippetsDatabaseTest, Delete) {
  CreateDatabase();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(db_inited());

  std::unique_ptr<NTPSnippet> snippet = CreateTestSnippet();

  db()->Save(*snippet);
  base::RunLoop().RunUntilIdle();

  db()->Delete(snippet->id());
  base::RunLoop().RunUntilIdle();

  db()->Load(base::Bind(&NTPSnippetsDatabaseTest::OnSnippetsLoaded,
                        base::Unretained(this)));

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(IsEmpty()));
  base::RunLoop().RunUntilIdle();
}

}  // namespace ntp_snippets
