// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/media_player_event_router.h"

#include "base/memory/singleton.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

static void BroadcastEvent(content::BrowserContext* context,
                           const std::string& event_name) {
  if (context && EventRouter::Get(context)) {
    scoped_ptr<base::ListValue> args(new base::ListValue());
    scoped_ptr<extensions::Event> event(new extensions::Event(
        event_name, args.Pass()));
    EventRouter::Get(context)->BroadcastEvent(event.Pass());
  }
}

MediaPlayerEventRouter::MediaPlayerEventRouter(content::BrowserContext* context)
    : browser_context_(context) {}

MediaPlayerEventRouter::~MediaPlayerEventRouter() {
}

void MediaPlayerEventRouter::NotifyNextTrack() {
  BroadcastEvent(browser_context_, "mediaPlayerPrivate.onNextTrack");
}

void MediaPlayerEventRouter::NotifyPrevTrack() {
  BroadcastEvent(browser_context_, "mediaPlayerPrivate.onPrevTrack");
}

void MediaPlayerEventRouter::NotifyTogglePlayState() {
  BroadcastEvent(browser_context_, "mediaPlayerPrivate.onTogglePlayState");
}

}  // namespace extensions
