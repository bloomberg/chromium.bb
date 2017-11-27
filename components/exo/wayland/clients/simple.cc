// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/simple.h"

#include <presentation-time-client-protocol.h>

#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {
namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* frame_callback_pending = static_cast<bool*>(data);
  *frame_callback_pending = false;
}

struct Frame {
  base::TimeTicks submit_time;
  std::unique_ptr<struct wp_presentation_feedback> feedback;
};

struct Presentation {
  base::circular_deque<Frame> submitted_frames;
  Simple::PresentationFeedback feedback;
};

void FeedbackSyncOutput(void* data,
                        struct wp_presentation_feedback* feedback,
                        wl_output* output) {}

void FeedbackPresented(void* data,
                       struct wp_presentation_feedback* feedback,
                       uint32_t tv_sec_hi,
                       uint32_t tv_sec_lo,
                       uint32_t tv_nsec,
                       uint32_t refresh,
                       uint32_t seq_hi,
                       uint32_t seq_lo,
                       uint32_t flags) {
  Presentation* presentation = static_cast<Presentation*>(data);
  DCHECK_GT(presentation->submitted_frames.size(), 0u);

  Frame& frame = presentation->submitted_frames.front();
  DCHECK_EQ(frame.feedback.get(), feedback);

  int64_t seconds = (static_cast<int64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  int64_t microseconds = seconds * base::Time::kMicrosecondsPerSecond +
                         tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  base::TimeTicks presentation_time =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(microseconds);
  presentation->feedback.total_presentation_latency +=
      presentation_time - frame.submit_time;
  ++presentation->feedback.num_frames_presented;
  presentation->submitted_frames.pop_front();
}

void FeedbackDiscarded(void* data, struct wp_presentation_feedback* feedback) {
  Presentation* presentation = static_cast<Presentation*>(data);
  DCHECK_GT(presentation->submitted_frames.size(), 0u);

  Frame& frame = presentation->submitted_frames.front();
  DCHECK_EQ(frame.feedback.get(), feedback);
  presentation->submitted_frames.pop_front();
}

}  // namespace

Simple::Simple() = default;

void Simple::Run(int frames, PresentationFeedback* feedback) {
  wl_callback_listener frame_listener = {FrameCallback};
  wp_presentation_feedback_listener feedback_listener = {
      FeedbackSyncOutput, FeedbackPresented, FeedbackDiscarded};

  Presentation presentation;
  int frame_count = 0;

  std::unique_ptr<wl_callback> frame_callback;
  bool frame_callback_pending = false;
  do {
    if (frame_callback_pending)
      continue;

    if (frame_count == frames)
      break;

    Buffer* buffer = DequeueBuffer();
    if (!buffer)
      continue;

    SkCanvas* canvas = buffer->sk_surface->getCanvas();

    static const SkColor kColors[] = {SK_ColorRED, SK_ColorBLACK};
    canvas->clear(kColors[++frame_count % arraysize(kColors)]);

    if (gr_context_) {
      gr_context_->flush();
      glFinish();
    }

    wl_surface_set_buffer_scale(surface_.get(), scale_);
    wl_surface_set_buffer_transform(surface_.get(), transform_);
    wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                      surface_size_.height());
    wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

    // Set up the frame callback.
    frame_callback_pending = true;
    frame_callback.reset(wl_surface_frame(surface_.get()));
    wl_callback_add_listener(frame_callback.get(), &frame_listener,
                             &frame_callback_pending);

    // Set up presentation feedback.
    Frame frame;
    frame.feedback.reset(
        wp_presentation_feedback(globals_.presentation.get(), surface_.get()));
    wp_presentation_feedback_add_listener(frame.feedback.get(),
                                          &feedback_listener, &presentation);
    frame.submit_time = base::TimeTicks::Now();
    presentation.submitted_frames.push_back(std::move(frame));

    wl_surface_commit(surface_.get());
    wl_display_flush(display_.get());
  } while (wl_display_dispatch(display_.get()) != -1);

  if (feedback)
    *feedback = presentation.feedback;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
