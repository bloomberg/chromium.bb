// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_shared_state.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "media/gpu/avda_codec_image.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

AVDASharedState::AVDASharedState(
    scoped_refptr<AVDASurfaceBundle> surface_bundle)
    : gl_matrix_{
          1, 0, 0, 0,  // Default to a sane guess just in case we can't get the
          0, 1, 0, 0,  // matrix on the first call. Will be Y-flipped later.
          0, 0, 1, 0,  //
          0, 0, 0, 1,  // Comment preserves 4x4 formatting.
      },
      surface_bundle_(surface_bundle),
      weak_this_factory_(this) {
  // If we're holding a reference to an overlay, then register to drop it if the
  // overlay's surface is destroyed.
  if (overlay()) {
    overlay()->AddSurfaceDestroyedCallback(base::Bind(
        &AVDASharedState::OnSurfaceDestroyed, weak_this_factory_.GetWeakPtr()));
  }
}

AVDASharedState::~AVDASharedState() = default;

void AVDASharedState::RenderCodecBufferToSurfaceTexture(
    MediaCodecBridge* codec,
    int codec_buffer_index) {
  if (surface_texture()->IsExpectingFrameAvailable())
    surface_texture()->WaitForFrameAvailable();
  codec->ReleaseOutputBuffer(codec_buffer_index, true);
  surface_texture()->SetReleaseTimeToNow();
}

void AVDASharedState::WaitForFrameAvailable() {
  surface_texture()->WaitForFrameAvailable();
}

void AVDASharedState::UpdateTexImage() {
  surface_texture()->UpdateTexImage();
  // Helpfully, this is already column major.
  surface_texture()->GetTransformMatrix(gl_matrix_);
}

void AVDASharedState::GetTransformMatrix(float matrix[16]) const {
  memcpy(matrix, gl_matrix_, sizeof(gl_matrix_));
}

void AVDASharedState::ClearReleaseTime() {
  if (surface_texture())
    surface_texture()->IgnorePendingRelease();
}

void AVDASharedState::OnSurfaceDestroyed(AndroidOverlay* overlay_raw) {
  if (surface_bundle_ && overlay() == overlay_raw)
    surface_bundle_ = nullptr;
}

}  // namespace media
