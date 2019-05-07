// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_experiment_manager.h"

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "media/base/media_controller.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ScopedPlayerState = MediaExperimentManager::ScopedPlayerState;

class MockExperimentClient : public MediaExperimentManager::Client {
 public:
  MOCK_METHOD1(OnExperimentStarted, void(const MediaPlayerId& player));
  MOCK_METHOD1(OnExperimentStopped, void(const MediaPlayerId& player));
};

class MediaExperimentManagerTest : public testing::Test {
 public:
  MediaExperimentManagerTest()
      : manager_(std::make_unique<MediaExperimentManager>()),
        player_id_1_(nullptr, 1),
        player_id_2_(nullptr, 2) {}

 protected:
  std::unique_ptr<MediaExperimentManager> manager_;
  // Unique player IDs.  Note that we can't CreateMediaPlayerIdForTesting()
  // since it doesn't return unique IDs.
  MediaPlayerId player_id_1_;
  MediaPlayerId player_id_2_;
};

TEST_F(MediaExperimentManagerTest, CreateAndDestroyPlayer) {
  MockExperimentClient client;

  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 0u);

  manager_->PlayerCreated(player_id_1_, &client);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 1u);

  manager_->PlayerCreated(player_id_2_, &client);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 2u);

  manager_->PlayerDestroyed(player_id_1_);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 1u);

  manager_->PlayerDestroyed(player_id_2_);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 0u);
}

TEST_F(MediaExperimentManagerTest, CreatePlayerAndDestroyClient) {
  // Create two players from one client, and make sure that destroying the
  // client destroys both players.
  MockExperimentClient client;

  manager_->PlayerCreated(player_id_1_, &client);
  manager_->PlayerCreated(player_id_2_, &client);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 2u);

  manager_->ClientDestroyed(&client);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 0u);
}

TEST_F(MediaExperimentManagerTest, CreateTwoClientsAndDestroyOneClient) {
  // Create one player each in two clients, and verify that destroying one
  // client destroys only one player.
  MockExperimentClient client_1;
  MockExperimentClient client_2;

  manager_->PlayerCreated(player_id_1_, &client_1);
  manager_->PlayerCreated(player_id_2_, &client_2);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 2u);

  manager_->ClientDestroyed(&client_1);
  EXPECT_EQ(manager_->GetPlayerCountForTesting(), 1u);
}

TEST_F(MediaExperimentManagerTest, ScopedPlayerStateModifiesState) {
  MockExperimentClient client_1;
  MockExperimentClient client_2;

  manager_->PlayerCreated(player_id_1_, &client_1);
  manager_->PlayerCreated(player_id_2_, &client_2);
  // Set the player state differently for each player.  We set two things so
  // that each player has a non-default value.
  {
    ScopedPlayerState state = manager_->GetPlayerState(player_id_1_);
    state->is_fullscreen = true;
    state->is_pip = false;
  }

  {
    ScopedPlayerState state = manager_->GetPlayerState(player_id_2_);
    state->is_fullscreen = false;
    state->is_pip = true;
  }

  // Make sure that the player state matches what we set for it.
  {
    ScopedPlayerState state = manager_->GetPlayerState(player_id_1_);
    EXPECT_TRUE(state->is_fullscreen);
    EXPECT_FALSE(state->is_pip);
  }

  {
    ScopedPlayerState state = manager_->GetPlayerState(player_id_2_);
    EXPECT_FALSE(state->is_fullscreen);
    EXPECT_TRUE(state->is_pip);
  }
}

TEST_F(MediaExperimentManagerTest, ScopedPlayerStateCallsCallback) {
  bool cb_called = false;
  base::OnceClosure cb = base::BindOnce([](bool* flag) { *flag = true; },
                                        base::Unretained(&cb_called));
  MediaExperimentManager::PlayerState state;
  state.is_fullscreen = false;

  // Normally, these would not by dynamically allocated.  However, this makes
  // it much easier to control when they're destroyed.  This is also why we
  // reference the underying state as (*scoped_1)-> ; we want tp use the
  // overloaded -> operator on ScopedPlayerState, not unique_ptr.
  std::unique_ptr<ScopedPlayerState> scoped_1 =
      std::make_unique<ScopedPlayerState>(std::move(cb), &state);
  (*scoped_1)->is_fullscreen = true;
  EXPECT_TRUE(state.is_fullscreen);
  EXPECT_FALSE(cb_called);

  // Moving |scoped_1| and deleting it should not call the callback.
  std::unique_ptr<ScopedPlayerState> scoped_2 =
      std::make_unique<ScopedPlayerState>(std::move(*scoped_1));
  scoped_1.reset();
  EXPECT_FALSE(cb_called);

  // |scoped_2| should now modify |state|.
  (*scoped_2)->is_fullscreen = false;
  EXPECT_FALSE(state.is_fullscreen);

  // Deleting |scoped_2| should call the callback.
  scoped_2.reset();
  EXPECT_TRUE(cb_called);
}

}  // namespace content
