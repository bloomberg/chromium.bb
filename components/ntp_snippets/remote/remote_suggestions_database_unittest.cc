// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_database.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Mock;
using testing::_;

namespace ntp_snippets {

bool operator==(const RemoteSuggestion& lhs, const RemoteSuggestion& rhs) {
  return lhs.id() == rhs.id() && lhs.title() == rhs.title() &&
         lhs.url() == rhs.url() &&
         lhs.publisher_name() == rhs.publisher_name() &&
         lhs.amp_url() == rhs.amp_url() && lhs.snippet() == rhs.snippet() &&
         lhs.salient_image_url() == rhs.salient_image_url() &&
         lhs.publish_date() == rhs.publish_date() &&
         lhs.expiry_date() == rhs.expiry_date() && lhs.score() == rhs.score() &&
         lhs.is_dismissed() == rhs.is_dismissed();
}

namespace {

std::unique_ptr<RemoteSuggestion> CreateTestSuggestion() {
  SnippetProto proto;
  proto.add_ids("http://localhost");
  proto.set_remote_category_id(1);  // Articles
  auto* source = proto.add_sources();
  source->set_url("http://localhost");
  source->set_publisher_name("Publisher");
  source->set_amp_url("http://amp");
  return RemoteSuggestion::CreateFromProto(proto);
}

// Eq matcher has to store the expected value, but RemoteSuggestion is movable-
// only.
MATCHER_P(PointeeEq, ptr_to_expected, "") {
  return *arg == *ptr_to_expected;
}

}  // namespace

class RemoteSuggestionsDatabaseTest : public testing::Test {
 public:
  RemoteSuggestionsDatabaseTest() {
    EXPECT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  ~RemoteSuggestionsDatabaseTest() override {
    // We need to run until idle after deleting the database, because
    // ProtoDatabaseImpl deletes the actual LevelDB asynchronously on the task
    // runner. Without this, we'd get reports of memory leaks.
    db_.reset();
    RunUntilIdle();
  }

  void CreateDatabase() {
    // Explicitly destroy any existing database first, so it releases the lock
    // on the file.
    db_.reset();

    db_.reset(new RemoteSuggestionsDatabase(database_dir_.GetPath()));
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  RemoteSuggestionsDatabase* db() { return db_.get(); }

  // TODO(tschumann): MOCK_METHODS on non mock objects are an anti-pattern.
  // Clean up.
  void OnSnippetsLoaded(RemoteSuggestion::PtrVector snippets) {
    OnSnippetsLoadedImpl(snippets);
  }
  MOCK_METHOD1(OnSnippetsLoadedImpl,
               void(const RemoteSuggestion::PtrVector& snippets));

  MOCK_METHOD1(OnImageLoaded, void(std::string));

 private:
  base::ScopedTempDir database_dir_;
  std::unique_ptr<RemoteSuggestionsDatabase> db_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsDatabaseTest);
};

TEST_F(RemoteSuggestionsDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadBeforeInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  // Start a snippet and image load before the DB is initialized.
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  db()->LoadImage("id",
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));

  // They should be serviced once initialization finishes.
  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  EXPECT_CALL(*this, OnImageLoaded(_));
  RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(RemoteSuggestionsDatabaseTest, LoadAfterInit) {
  CreateDatabase();
  EXPECT_FALSE(db()->IsInitialized());

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_)).Times(0);
  RunUntilIdle();
  EXPECT_TRUE(db()->IsInitialized());

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnSnippetsLoadedImpl(_));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(_));
  db()->LoadImage("id",
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Save) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, DISABLED_SavePersist) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and an image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  RunUntilIdle();

  // They should still exist after recreating the database.
  CreateDatabase();

  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Update) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();

  // Store a snippet.
  db()->SaveSnippet(*snippet);

  // Change it.
  snippet->set_dismissed(true);
  db()->SaveSnippet(*snippet);

  // Make sure we get the updated version.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, Delete) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();

  // Store a snippet.
  db()->SaveSnippet(*snippet);

  // Make sure it's there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());

  // Make sure it's gone.
  EXPECT_CALL(*this, OnSnippetsLoadedImpl(IsEmpty()));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteSnippetDoesNotDeleteImage) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store a snippet and image.
  db()->SaveSnippet(*snippet);
  db()->SaveImage(snippet->id(), image_data);
  RunUntilIdle();

  // Make sure they're there.
  EXPECT_CALL(*this,
              OnSnippetsLoadedImpl(ElementsAre(PointeeEq(snippet.get()))));
  db()->LoadSnippets(
      base::Bind(&RemoteSuggestionsDatabaseTest::OnSnippetsLoaded,
                 base::Unretained(this)));
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteSnippet(snippet->id());

  // Make sure the image is still there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(RemoteSuggestionsDatabaseTest, DeleteImage) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  std::unique_ptr<RemoteSuggestion> snippet = CreateTestSuggestion();
  std::string image_data("pretty image");

  // Store the image.
  db()->SaveImage(snippet->id(), image_data);
  RunUntilIdle();

  // Make sure the image is there.
  EXPECT_CALL(*this, OnImageLoaded(image_data));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();

  Mock::VerifyAndClearExpectations(this);

  // Delete the snippet.
  db()->DeleteImage(snippet->id());

  // Make sure the image is gone.
  EXPECT_CALL(*this, OnImageLoaded(std::string()));
  db()->LoadImage(snippet->id(),
                  base::Bind(&RemoteSuggestionsDatabaseTest::OnImageLoaded,
                             base::Unretained(this)));
  RunUntilIdle();
}

namespace {

void LoadExpectedImage(RemoteSuggestionsDatabase* db,
                       const std::string& id,
                       const std::string& expected_data) {
  base::RunLoop run_loop;
  db->LoadImage(id, base::Bind(
                        [](base::Closure signal, std::string expected_data,
                           std::string actual_data) {
                          EXPECT_THAT(actual_data, Eq(expected_data));
                          signal.Run();
                        },
                        run_loop.QuitClosure(), expected_data));
  run_loop.Run();
}

}  // namespace

TEST_F(RemoteSuggestionsDatabaseTest, ShouldGarbageCollectImages) {
  CreateDatabase();
  RunUntilIdle();
  ASSERT_TRUE(db()->IsInitialized());

  // Store images.
  db()->SaveImage("snippet-id-1", "pretty-image-1");
  db()->SaveImage("snippet-id-2", "pretty-image-2");
  db()->SaveImage("snippet-id-3", "pretty-image-3");
  RunUntilIdle();

  // Make sure the to-be-garbage collected images are there.
  LoadExpectedImage(db(), "snippet-id-1", "pretty-image-1");
  LoadExpectedImage(db(), "snippet-id-3", "pretty-image-3");

  // Garbage collect all except the second.
  db()->GarbageCollectImages(base::MakeUnique<std::set<std::string>>(
      std::set<std::string>({"snippet-id-2"})));
  RunUntilIdle();

  // Make sure the images are gone.
  LoadExpectedImage(db(), "snippet-id-1", "");
  LoadExpectedImage(db(), "snippet-id-3", "");
  // Make sure the second still exists.
  LoadExpectedImage(db(), "snippet-id-2", "pretty-image-2");
}

}  // namespace ntp_snippets
