// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/null_video_renderer.h"

namespace media {

NullVideoRenderer::NullVideoRenderer() {}
NullVideoRenderer::~NullVideoRenderer() {}

bool NullVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  return true;
}

void NullVideoRenderer::OnStop(media::FilterCallback* callback) {
  callback->Run();
  delete callback;
}

void NullVideoRenderer::OnFrameAvailable() {
  // Do nothing.
}

}  // namespace media
