// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/player_wtl/wtl_renderer.h"

#include "media/tools/player_wtl/view.h"

WtlVideoRenderer::WtlVideoRenderer(WtlVideoWindow* window)
    : window_(window) {
}

WtlVideoRenderer::~WtlVideoRenderer() {}

bool WtlVideoRenderer::OnInitialize(media::VideoDecoder* decoder) {
  window_->SetSize(decoder->width(), decoder->height());
  return true;
}

void WtlVideoRenderer::OnStop(media::FilterCallback* callback) {
  if (callback) {
    callback->Run();
    delete callback;
  }
}

void WtlVideoRenderer::OnFrameAvailable() {
  window_->Invalidate();
}
