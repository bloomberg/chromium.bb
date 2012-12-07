// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/media_player_event_router.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

static void BroadcastEvent(Profile* profile, const std::string& event_name) {
  if (profile && extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<ListValue> args(new ListValue());
    scoped_ptr<extensions::Event> event(new extensions::Event(
        event_name, args.Pass()));
    extensions::ExtensionSystem::Get(profile)->event_router()->
        BroadcastEvent(event.Pass());
  }
}

ExtensionMediaPlayerEventRouter::ExtensionMediaPlayerEventRouter()
    : profile_(NULL) {
}

ExtensionMediaPlayerEventRouter*
    ExtensionMediaPlayerEventRouter::GetInstance() {
  return Singleton<ExtensionMediaPlayerEventRouter>::get();
}

void ExtensionMediaPlayerEventRouter::Init(Profile* profile) {
  profile_ = profile;
}

void ExtensionMediaPlayerEventRouter::NotifyNextTrack() {
  BroadcastEvent(profile_, "mediaPlayerPrivate.onNextTrack");
}

void ExtensionMediaPlayerEventRouter::NotifyPlaylistChanged() {
  BroadcastEvent(profile_, "mediaPlayerPrivate.onPlaylistChanged");
}

void ExtensionMediaPlayerEventRouter::NotifyPrevTrack() {
  BroadcastEvent(profile_, "mediaPlayerPrivate.onPrevTrack");
}

void ExtensionMediaPlayerEventRouter::NotifyTogglePlayState() {
  BroadcastEvent(profile_, "mediaPlayerPrivate.onTogglePlayState");
}
