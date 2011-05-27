// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_mediaplayer_private_api.h"

#include "base/values.h"
#include "chrome/browser/chromeos/media/media_player.h"

static const char kPropertyPath[] = "path";
static const char kPropertyForce[] = "force";
static const char kPropertyItems[] = "items";
static const char kPropertyPosition[] = "position";
static const char kPropertyError[] = "error";
static const char kPropertyPendingPlaybackRequest[] =
    "pendingPlaybackRequest";

bool PlayAtMediaplayerFunction::RunImpl() {
  int position;
  if (!args_->GetInteger(0, &position))
    return false;
  MediaPlayer::GetInstance()->SetPlaybackRequest();
  MediaPlayer::GetInstance()->SetPlaylistPosition(position);
  return true;
}

static ListValue* GetPlaylistItems() {
  ListValue* result = new ListValue();

  MediaPlayer::UrlVector const& src = MediaPlayer::GetInstance()->GetPlaylist();

  for (size_t i = 0; i < src.size(); i++) {
    DictionaryValue* url_value = new DictionaryValue();
    url_value->SetString(kPropertyPath, src[i].url.spec());
    url_value->SetBoolean(kPropertyError, src[i].haderror);
    result->Append(url_value);
  }
  return result;
}

bool GetPlaylistMediaplayerFunction::RunImpl() {
  bool reset_pending_playback_request;
  if (!args_->GetBoolean(0, &reset_pending_playback_request))
    return false;

  DictionaryValue* result = new DictionaryValue();
  MediaPlayer* player = MediaPlayer::GetInstance();

  result->Set(kPropertyItems, GetPlaylistItems());
  result->SetInteger(kPropertyPosition, player->GetPlaylistPosition());
  if (reset_pending_playback_request) {
    result->SetBoolean(kPropertyPendingPlaybackRequest,
                       player->GetPendingPlayRequestAndReset());
  }

  result_.reset(result);
  return true;
}

bool SetPlaybackErrorMediaplayerFunction::RunImpl() {
  std::string url;
  // Get path string.
  if (args_->GetString(0, &url))
    MediaPlayer::GetInstance()->SetPlaybackError(GURL(url));
  return true;
}

bool TogglePlaylistPanelMediaplayerFunction::RunImpl() {
  MediaPlayer::GetInstance()->TogglePlaylistWindowVisible();
  return true;
}

bool ToggleFullscreenMediaplayerFunction::RunImpl() {
  MediaPlayer::GetInstance()->ToggleFullscreen();
  return true;
}
