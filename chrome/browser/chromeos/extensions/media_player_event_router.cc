// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/media_player_event_router.h"

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

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
  if (profile_ && extensions::ExtensionSystem::Get(profile_)->event_router()) {
    scoped_ptr<ListValue> args(new ListValue());
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToRenderers("mediaPlayerPrivate.onNextTrack", args.Pass(),
                                 NULL, GURL());
  }
}

void ExtensionMediaPlayerEventRouter::NotifyPlaylistChanged() {
  if (profile_ && extensions::ExtensionSystem::Get(profile_)->event_router()) {
    scoped_ptr<ListValue> args(new ListValue());
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToRenderers("mediaPlayerPrivate.onPlaylistChanged",
                                 args.Pass(), NULL, GURL());
  }
}

void ExtensionMediaPlayerEventRouter::NotifyPrevTrack() {
  if (profile_ && extensions::ExtensionSystem::Get(profile_)->event_router()) {
    scoped_ptr<ListValue> args(new ListValue());
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToRenderers("mediaPlayerPrivate.onPrevTrack", args.Pass(),
                                 NULL, GURL());
  }
}

void ExtensionMediaPlayerEventRouter::NotifyTogglePlayState() {
  if (profile_ && extensions::ExtensionSystem::Get(profile_)->event_router()) {
    scoped_ptr<ListValue> args(new ListValue());
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToRenderers("mediaPlayerPrivate.onTogglePlayState",
                                 args.Pass(), NULL, GURL());
  }
}
