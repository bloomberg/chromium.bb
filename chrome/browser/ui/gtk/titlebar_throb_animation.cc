// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/titlebar_throb_animation.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// We don't bother to clean up these or the pixbufs they contain when we exit.
std::vector<GdkPixbuf*>* g_throbber_frames = NULL;
std::vector<GdkPixbuf*>* g_throbber_waiting_frames = NULL;

// Load |resource_id| from the ResourceBundle and split it into a series of
// square GdkPixbufs that get stored in |frames|.
void MakeThrobberFrames(int resource_id, std::vector<GdkPixbuf*>* frames) {
  ui::ResourceBundle &rb = ui::ResourceBundle::GetSharedInstance();
  SkBitmap frame_strip = rb.GetImageNamed(resource_id).AsBitmap();

  // Each frame of the animation is a square, so we use the height as the
  // frame size.
  int frame_size = frame_strip.height();
  size_t num_frames = frame_strip.width() / frame_size;

  // Make a separate GdkPixbuf for each frame of the animation.
  for (size_t i = 0; i < num_frames; ++i) {
    SkBitmap frame = SkBitmapOperations::CreateTiledBitmap(frame_strip,
        i * frame_size, 0, frame_size, frame_size);
    frames->push_back(gfx::GdkPixbufFromSkBitmap(frame));
  }
}

}  // namespace

// TODO(tc): Handle anti-clockwise spinning when waiting for a connection.

TitlebarThrobAnimation::TitlebarThrobAnimation()
    : current_frame_(0),
      current_waiting_frame_(0) {
}

GdkPixbuf* TitlebarThrobAnimation::GetNextFrame(bool is_waiting) {
  InitFrames();
  if (is_waiting) {
    return (*g_throbber_waiting_frames)[current_waiting_frame_++ %
        g_throbber_waiting_frames->size()];
  } else {
    return (*g_throbber_frames)[current_frame_++ % g_throbber_frames->size()];
  }
}

void TitlebarThrobAnimation::Reset() {
  current_frame_ = 0;
  current_waiting_frame_ = 0;
}

// static
void TitlebarThrobAnimation::InitFrames() {
  if (g_throbber_frames)
    return;

  // We load the light version of the throbber since it'll be in the titlebar.
  g_throbber_frames = new std::vector<GdkPixbuf*>;
  MakeThrobberFrames(IDR_THROBBER_LIGHT, g_throbber_frames);

  g_throbber_waiting_frames = new std::vector<GdkPixbuf*>;
  MakeThrobberFrames(IDR_THROBBER_WAITING_LIGHT, g_throbber_waiting_frames);
}
