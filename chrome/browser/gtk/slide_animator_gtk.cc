// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/slide_animator_gtk.h"

#include "app/animation.h"
#include "app/slide_animation.h"
#include "base/logging.h"

namespace {

void OnFixedSizeAllocate(GtkWidget* fixed,
                         GtkAllocation* allocation,
                         GtkWidget* child) {
  // The size of the GtkFixed has changed. We want |child_| to match widths,
  // but the height should not change.
  gtk_widget_set_size_request(child, allocation->width, -1);
}

}  // namespace

SlideAnimatorGtk::SlideAnimatorGtk(GtkWidget* child,
                                   Direction direction,
                                   int duration,
                                   bool linear,
                                   bool control_child_size,
                                   Delegate* delegate)
    : child_(child),
      direction_(direction),
      delegate_(delegate) {
  widget_.Own(gtk_fixed_new());
  gtk_fixed_put(GTK_FIXED(widget_.get()), child, 0, 0);
  gtk_widget_set_size_request(widget_.get(), -1, 0);
  if (control_child_size) {
    // If the child requests it, we will manually set the size request for
    // |child_| every time the GtkFixed changes sizes. This is mainly useful
    // for bars, where we want the child to expand to fill all available space.
    g_signal_connect(widget_.get(), "size-allocate",
                     G_CALLBACK(OnFixedSizeAllocate), child_);
  }

  // We connect to this signal to set an initial position for our child widget.
  // The reason we connect to this signal rather than setting the initial
  // position here is that the widget is currently unallocated and may not
  // even have a size request.
  g_signal_connect(child, "size-allocate",
                   G_CALLBACK(OnChildSizeAllocate), this);

  child_needs_move_ = (direction == DOWN);

  animation_.reset(new SlideAnimation(this));
  // Default tween type is EASE_OUT.
  if (linear)
    animation_->SetTweenType(SlideAnimation::NONE);
  if (duration != 0)
    animation_->SetSlideDuration(duration);
}

SlideAnimatorGtk::~SlideAnimatorGtk() {
  widget_.Destroy();
}

void SlideAnimatorGtk::Open() {
  gtk_widget_show(widget_.get());
  animation_->Show();
}

void SlideAnimatorGtk::OpenWithoutAnimation() {
  animation_->Reset(1.0);
  Open();
  AnimationProgressed(animation_.get());
}

void SlideAnimatorGtk::Close() {
  animation_->Hide();
}

void SlideAnimatorGtk::End() {
  animation_->End();
}

void SlideAnimatorGtk::CloseWithoutAnimation() {
  animation_->Reset(0.0);
  animation_->Hide();
  AnimationProgressed(animation_.get());
  gtk_widget_hide(widget_.get());
}

bool SlideAnimatorGtk::IsShowing() {
  return animation_->IsShowing();
}

bool SlideAnimatorGtk::IsClosing() {
  return animation_->IsClosing();
}

bool SlideAnimatorGtk::IsAnimating() {
  return animation_->IsAnimating();
}

void SlideAnimatorGtk::AnimationProgressed(const Animation* animation) {
  GtkRequisition req;
  gtk_widget_size_request(child_, &req);

  int showing_height = static_cast<int>(req.height *
                                        animation_->GetCurrentValue());
  if (direction_ == DOWN) {
    gtk_fixed_move(GTK_FIXED(widget_.get()), child_, 0,
                   showing_height - req.height);
    child_needs_move_ = false;
  }
  gtk_widget_set_size_request(widget_.get(), -1, showing_height);
}

void SlideAnimatorGtk::AnimationEnded(const Animation* animation) {
  if (!animation_->IsShowing()) {
    gtk_widget_hide(widget_.get());
    if (delegate_)
      delegate_->Closed();
  }
}

// static
void SlideAnimatorGtk::OnChildSizeAllocate(GtkWidget* child,
                                           GtkAllocation* allocation,
                                           SlideAnimatorGtk* slider) {
  if (slider->child_needs_move_) {
    gtk_fixed_move(GTK_FIXED(slider->widget()), child, 0, -allocation->height);
    slider->child_needs_move_ = false;
  }
}
