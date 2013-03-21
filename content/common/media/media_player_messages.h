// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for android media player.
// Multiply-included message file, hence no include guard.

#include <string>

#include "base/time.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerMsgStart

// Messages for notifying the render process of media playback status -------

// Media buffering has updated.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaBufferingUpdate,
                    int /* player_id */,
                    int /* percent */)

// A media playback error has occured.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaError,
                    int /* player_id */,
                    int /* error */)

// Playback is completed.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_MediaPlaybackCompleted,
                    int /* player_id */)

// Player is prepared.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaPrepared,
                    int /* player_id */,
                    base::TimeDelta /* duration */)

// Media seek is completed.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaSeekCompleted,
                    int /* player_id */,
                    base::TimeDelta /* current_time */)

// Video size has changed.
IPC_MESSAGE_ROUTED3(MediaPlayerMsg_MediaVideoSizeChanged,
                    int /* player_id */,
                    int /* width */,
                    int /* height */)

// The current play time has updated.
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_MediaTimeUpdate,
                    int /* player_id */,
                    base::TimeDelta /* current_time */)

// The player has been released.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_MediaPlayerReleased,
                    int /* player_id */)

// The player has entered fullscreen mode.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidEnterFullscreen,
                    int /* player_id */)

// The player exited fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidExitFullscreen,
                    int /* player_id */)

// The player started playing.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidMediaPlayerPlay,
                    int /* player_id */)

// The player was paused.
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_DidMediaPlayerPause,
                    int /* player_id */)

// Messages for controllering the media playback in browser process ----------

// Destroy the media player object.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_DestroyMediaPlayer,
                    int /* player_id */)

// Destroy all the players.
IPC_MESSAGE_ROUTED0(MediaPlayerHostMsg_DestroyAllMediaPlayers)

// Initialize a media player object with the given player_id.
IPC_MESSAGE_ROUTED3(MediaPlayerHostMsg_MediaPlayerInitialize,
                    int /* player_id */,
                    GURL /* url */,
                    GURL /* first_party_for_cookies */)

// Pause the player.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_MediaPlayerPause,
                    int /* player_id */)

// Release player resources, but keep the object for future usage.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_MediaPlayerRelease,
                    int /* player_id */)

// Perform a seek.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_MediaPlayerSeek,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Start the player for playback.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_MediaPlayerStart,
                    int /* player_id */)

// Request the player to enter fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_EnterFullscreen,
                    int /* player_id */)

// Request the player to exit fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_ExitFullscreen,
                    int /* player_id */)

// Request the player to use external surface for rendering.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_RequestExternalSurface,
                    int /* player_id */)
