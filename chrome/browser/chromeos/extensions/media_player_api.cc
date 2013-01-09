// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/media_player_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/media_player_event_router.h"
#include "chrome/browser/chromeos/media/media_player.h"

namespace {

static const char kPropertyItems[] = "items";
static const char kPropertyPosition[] = "position";

}  // namespace

namespace extensions {

bool PlayMediaplayerFunction::RunImpl() {
  if (args_->GetSize() < 2) {
    return false;
  }

  ListValue* url_list = NULL;
  if (!args_->GetList(0, &url_list))
    return false;

  int position;
  if (!args_->GetInteger(1, &position))
    return false;

  MediaPlayer* player = MediaPlayer::GetInstance();

  player->PopupMediaPlayer();
  player->ClearPlaylist();

  size_t len = url_list->GetSize();
  for (size_t i = 0; i < len; ++i) {
    std::string path;
    url_list->GetString(i, &path);
    player->EnqueueMediaFileUrl(GURL(path));
  }

  player->SetPlaylistPosition(position);
  player->NotifyPlaylistChanged();

  return true;
}

static ListValue* GetPlaylistItems() {
  ListValue* result = new ListValue();

  MediaPlayer::UrlVector const& src = MediaPlayer::GetInstance()->GetPlaylist();

  for (size_t i = 0; i < src.size(); i++) {
    result->Append(new base::StringValue(src[i].spec()));
  }
  return result;
}

bool GetPlaylistMediaplayerFunction::RunImpl() {
  DictionaryValue* result = new DictionaryValue();
  MediaPlayer* player = MediaPlayer::GetInstance();

  result->Set(kPropertyItems, GetPlaylistItems());
  result->SetInteger(kPropertyPosition, player->GetPlaylistPosition());

  SetResult(result);
  return true;
}

// TODO(kaznacheev): rename the API method to adjustWindowHeight here and in
// media_player_private.json.
bool SetWindowHeightMediaplayerFunction::RunImpl() {
  int height_diff;
  if (!args_->GetInteger(0, &height_diff))
    return false;
  MediaPlayer::GetInstance()->AdjustWindowHeight(height_diff);
  return true;
}

bool CloseWindowMediaplayerFunction::RunImpl() {
  MediaPlayer::GetInstance()->CloseWindow();
  return true;
}

MediaPlayerAPI::MediaPlayerAPI(Profile* profile)
    : profile_(profile) {
}

MediaPlayerAPI::~MediaPlayerAPI() {
}

// static
MediaPlayerAPI* MediaPlayerAPI::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<MediaPlayerAPI>::GetForProfile(profile);
}

MediaPlayerEventRouter* MediaPlayerAPI::media_player_event_router() {
  if (!media_player_event_router_)
    media_player_event_router_.reset(new MediaPlayerEventRouter(profile_));
  return media_player_event_router_.get();
}

static base::LazyInstance<ProfileKeyedAPIFactory<MediaPlayerAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<MediaPlayerAPI>* MediaPlayerAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
