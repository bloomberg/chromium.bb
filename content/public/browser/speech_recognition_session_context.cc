// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_context.h"

#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"

namespace content {

SpeechRecognitionSessionContext::SpeechRecognitionSessionContext()
    : render_process_id(ChildProcessHost::kInvalidUniqueID),
      render_view_id(MSG_ROUTING_NONE),
      render_frame_id(MSG_ROUTING_NONE),
      embedder_render_process_id(ChildProcessHost::kInvalidUniqueID),
      embedder_render_view_id(MSG_ROUTING_NONE),
      request_id(0) {}

SpeechRecognitionSessionContext::SpeechRecognitionSessionContext(
    const SpeechRecognitionSessionContext& other) = default;

SpeechRecognitionSessionContext::~SpeechRecognitionSessionContext() {
}

}  // namespace content
