// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/codec_surface_bundle.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/android/android_overlay.h"

namespace media {

CodecSurfaceBundle::CodecSurfaceBundle()
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {}

CodecSurfaceBundle::CodecSurfaceBundle(std::unique_ptr<AndroidOverlay> overlay)
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()),
      overlay_(std::move(overlay)),
      weak_factory_(this) {}

CodecSurfaceBundle::CodecSurfaceBundle(
    scoped_refptr<TextureOwner> texture_owner)
    : RefCountedDeleteOnSequence<CodecSurfaceBundle>(
          base::SequencedTaskRunnerHandle::Get()),
      texture_owner_(std::move(texture_owner)),
      texture_owner_surface_(texture_owner_->CreateJavaSurface()),
      weak_factory_(this) {}

CodecSurfaceBundle::~CodecSurfaceBundle() {
  // Explicitly free the surface first, just to be sure that it's deleted before
  // the TextureOwner is.
  texture_owner_surface_ = gl::ScopedJavaSurface();

  // Also release the back buffers.
  if (!texture_owner_)
    return;

  auto task_runner = texture_owner_->task_runner();
  if (task_runner->RunsTasksInCurrentSequence()) {
    texture_owner_->ReleaseBackBuffers();
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindRepeating(&TextureOwner::ReleaseBackBuffers, texture_owner_));
  }
}

const base::android::JavaRef<jobject>& CodecSurfaceBundle::GetJavaSurface()
    const {
  return overlay_ ? overlay_->GetJavaSurface()
                  : texture_owner_surface_.j_surface();
}

CodecSurfaceBundle::ScheduleLayoutCB CodecSurfaceBundle::GetScheduleLayoutCB() {
  return base::BindRepeating(&CodecSurfaceBundle::ScheduleLayout,
                             weak_factory_.GetWeakPtr());
}

void CodecSurfaceBundle::ScheduleLayout(const gfx::Rect& rect) {
  if (layout_rect_ == rect)
    return;
  layout_rect_ = rect;

  if (overlay_)
    overlay_->ScheduleLayout(rect);
}

}  // namespace media
