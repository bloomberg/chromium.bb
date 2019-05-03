// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_experiment_manager.h"

namespace content {

MediaExperimentManager::MediaExperimentManager() = default;

MediaExperimentManager::~MediaExperimentManager() = default;

// Keeps track of all media players across all pages, and notifies them when
// they enter or leave an active experiment.
void MediaExperimentManager::PlayerCreated(const MediaPlayerId& player_id,
                                           Client* client) {
  // TODO: check that we don't know about it already.
  player_ids_by_client_[client].insert(player_id);
  players_[player_id].client = client;
}

void MediaExperimentManager::PlayerDestroyed(const MediaPlayerId& player_id) {
  ErasePlayersInternal({player_id});
}

void MediaExperimentManager::ClientDestroyed(Client* client) {
  auto by_client = player_ids_by_client_.find(client);
  // Make a copy, since ErasePlayers will modify the original.
  ErasePlayersInternal(std::set<MediaPlayerId>(by_client->second));
}

void MediaExperimentManager::ErasePlayersInternal(
    const std::set<MediaPlayerId>& player_ids) {
  for (auto player_id : player_ids) {
    auto player_iter = players_.find(player_id);
    DCHECK(player_iter != players_.end());

    // Erase this player from the client, and maybe the client if it's the
    // only player owned by it.
    auto by_client_iter =
        player_ids_by_client_.find(player_iter->second.client);
    DCHECK(by_client_iter != player_ids_by_client_.end());
    by_client_iter->second.erase(player_id);
    if (by_client_iter->second.size() == 0)
      player_ids_by_client_.erase(by_client_iter);

    players_.erase(player_iter);
  }
}

size_t MediaExperimentManager::GetPlayerCountForTesting() const {
  return players_.size();
}

// static
MediaExperimentManager* MediaExperimentManager::Instance() {
  return nullptr;
}

}  // namespace content
