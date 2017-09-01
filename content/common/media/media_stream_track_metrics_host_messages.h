// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_MESSAGES_H_

#include <stdint.h>

#include "base/values.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaStreamTrackMetricsHostMsgStart

// Messages sent to MediaStreamTrackMetricsHost.
IPC_MESSAGE_CONTROL3(MediaStreamTrackMetricsHost_AddTrack,
                     uint64_t /* id */,
                     bool /* is_audio */,
                     bool /* is_remote */)
IPC_MESSAGE_CONTROL1(MediaStreamTrackMetricsHost_RemoveTrack, uint64_t /* id */)

#endif  // CONTENT_COMMON_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_MESSAGES_H_
