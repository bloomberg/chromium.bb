// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for android media player.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_player_android.h"
#include "media/base/media_keys.h"
#include "ui/gfx/rect_f.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerMsgStart

#include "media/base/android/demuxer_stream_player_params.h"

IPC_ENUM_TRAITS(media::AudioCodec)
IPC_ENUM_TRAITS(media::DemuxerStream::Status)
IPC_ENUM_TRAITS(media::DemuxerStream::Type)
IPC_ENUM_TRAITS(media::MediaKeys::KeyError)
IPC_ENUM_TRAITS(media::VideoCodec)

IPC_STRUCT_TRAITS_BEGIN(media::MediaPlayerHostMsg_DemuxerReady_Params)
  IPC_STRUCT_TRAITS_MEMBER(audio_codec)
  IPC_STRUCT_TRAITS_MEMBER(audio_channels)
  IPC_STRUCT_TRAITS_MEMBER(audio_sampling_rate)
  IPC_STRUCT_TRAITS_MEMBER(is_audio_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(audio_extra_data)

  IPC_STRUCT_TRAITS_MEMBER(video_codec)
  IPC_STRUCT_TRAITS_MEMBER(video_size)
  IPC_STRUCT_TRAITS_MEMBER(is_video_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(video_extra_data)

  IPC_STRUCT_TRAITS_MEMBER(duration_ms)
  IPC_STRUCT_TRAITS_MEMBER(key_system)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(access_units)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::AccessUnit)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(end_of_stream)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(key_id)
  IPC_STRUCT_TRAITS_MEMBER(iv)
  IPC_STRUCT_TRAITS_MEMBER(subsamples)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(media::MediaPlayerAndroid::SourceType)

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

// Media metadata has changed.
IPC_MESSAGE_ROUTED5(MediaPlayerMsg_MediaMetadataChanged,
                    int /* player_id */,
                    base::TimeDelta /* duration */,
                    int /* width */,
                    int /* height */,
                    bool /* success */)

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

// Media seek is requested.
IPC_MESSAGE_ROUTED3(MediaPlayerMsg_MediaSeekRequest,
                    int /* player_id */,
                    base::TimeDelta /* time_to_seek */,
                    uint32 /* seek_request_id */)

// The media source player reads data from demuxer
IPC_MESSAGE_ROUTED2(MediaPlayerMsg_ReadFromDemuxer,
                    int /* player_id */,
                    media::DemuxerStream::Type /* type */)

// The player needs new config data
IPC_MESSAGE_ROUTED1(MediaPlayerMsg_MediaConfigRequest,
                    int /* player_id */)

// Messages for controllering the media playback in browser process ----------

// Destroy the media player object.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_DestroyMediaPlayer,
                    int /* player_id */)

// Destroy all the players.
IPC_MESSAGE_ROUTED0(MediaPlayerHostMsg_DestroyAllMediaPlayers)

// Initialize a media player object with the given player_id.
IPC_MESSAGE_ROUTED4(
    MediaPlayerHostMsg_Initialize,
    int /* player_id */,
    GURL /* url */,
    media::MediaPlayerAndroid::SourceType /* source_type */,
    GURL /* first_party_for_cookies */)

// Pause the player.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_Pause, int /* player_id */)

// Release player resources, but keep the object for future usage.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_Release, int /* player_id */)

// Perform a seek.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_Seek,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Start the player for playback.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_Start, int /* player_id */)

// Start the player for playback.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_SetVolume,
                    int /* player_id */,
                    double /* volume */)

// Request the player to enter fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_EnterFullscreen, int /* player_id */)

// Request the player to exit fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerHostMsg_ExitFullscreen, int /* player_id */)

// Sent when the seek request is received by the WebMediaPlayerAndroid.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_MediaSeekRequestAck,
                    int /* player_id */,
                    uint32 /* seek_request_id */)

// Inform the media source player that the demuxer is ready.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_DemuxerReady,
                    int /* player_id */,
                    media::MediaPlayerHostMsg_DemuxerReady_Params)

// Sent when the data was read from the ChunkDemuxer.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_ReadFromDemuxerAck,
                    int /* player_id */,
                    media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params)

// Inform the media source player of changed media duration from demuxer.
IPC_MESSAGE_ROUTED2(MediaPlayerHostMsg_DurationChanged,
                    int /* player_id */,
                    base::TimeDelta /* duration */)

#if defined(GOOGLE_TV)
// Notify the player about the external surface, requesting it if necessary.
IPC_MESSAGE_ROUTED3(MediaPlayerHostMsg_NotifyExternalSurface,
                    int /* player_id */,
                    bool /* is_request */,
                    gfx::RectF /* rect */)

#endif

// Messages for encrypted media extensions API ------------------------------
// TODO(xhwang): Move the following messages to a separate file.

IPC_MESSAGE_ROUTED2(MediaKeysHostMsg_InitializeCDM,
                    int /* media_keys_id */,
                    std::vector<uint8> /* uuid */)

IPC_MESSAGE_ROUTED3(MediaKeysHostMsg_GenerateKeyRequest,
                    int /* media_keys_id */,
                    std::string /* type */,
                    std::vector<uint8> /* init_data */)

IPC_MESSAGE_ROUTED4(MediaKeysHostMsg_AddKey,
                    int /* media_keys_id */,
                    std::vector<uint8> /* key */,
                    std::vector<uint8> /* init_data */,
                    std::string /* session_id */)

IPC_MESSAGE_ROUTED2(MediaKeysHostMsg_CancelKeyRequest,
                    int /* media_keys_id */,
                    std::string /* session_id */)

IPC_MESSAGE_ROUTED2(MediaKeysMsg_KeyAdded,
                    int /* media_keys_id */,
                    std::string /* session_id */)

IPC_MESSAGE_ROUTED4(MediaKeysMsg_KeyError,
                    int /* media_keys_id */,
                    std::string /* session_id */,
                    media::MediaKeys::KeyError /* error_code */,
                    int /* system_code */)

IPC_MESSAGE_ROUTED4(MediaKeysMsg_KeyMessage,
                    int /* media_keys_id */,
                    std::string /* session_id */,
                    std::vector<uint8> /* message */,
                    std::string /* destination_url */)
