// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/chromeos/media/media_player.h"
#include "chrome/browser/chromeos/media/media_player_extension_api.h"

static const char kPropertyItems[] = "items";
static const char kPropertyPosition[] = "position";

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
    result->Append(Value::CreateStringValue(src[i].spec()));
  }
  return result;
}

bool GetPlaylistMediaplayerFunction::RunImpl() {
  DictionaryValue* result = new DictionaryValue();
  MediaPlayer* player = MediaPlayer::GetInstance();

  result->Set(kPropertyItems, GetPlaylistItems());
  result->SetInteger(kPropertyPosition, player->GetPlaylistPosition());

  result_.reset(result);
  return true;
}

bool SetWindowHeightMediaplayerFunction::RunImpl() {
  int height;
  if (!args_->GetInteger(0, &height))
    return false;
  MediaPlayer::GetInstance()->SetWindowHeight(height);
  return true;
}

bool CloseWindowMediaplayerFunction::RunImpl() {
  MediaPlayer::GetInstance()->CloseWindow();
  return true;
}
