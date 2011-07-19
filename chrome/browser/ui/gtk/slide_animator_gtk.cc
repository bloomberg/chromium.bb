// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/slide_animator_gtk.h"

#include "chrome/browser/ui/gtk/gtk_expanded_container.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/slide_animation.h"

namespace {

void OnChildSizeRequest(GtkWidget* expanded,
                        GtkWidget* child,
                        GtkRequisition* requisition,
                        gpointer control_child_size) {
  // If |control_child_size| is true, then we want |child_| to match the width
  // of the |widget_|, but the height of |child_| should not change.
  if (!GPOINTER_TO_INT(control_child_size)) {
    requisition->width = -1;
  }
  requisition->height = -1;
}

}  // namespace

bool SlideAnimatorGtk::animations_enabled_ = true;

SlideAnimatorGtk::SlideAnimatorGtk(GtkWidget* child,
                                   Direction direction,
                                   int duration,
                                   bool linear,
                                   bool control_child_size,
                                   Delegate* delegate)
    : child_(child),
      direction_(direction),
      delegate_(delegate) {
  widget_.Own(gtk_expanded_container_new());
  gtk_container_add(GTK_CONTAINER(widget_.get()), child);
  gtk_widget_set_size_request(widget_.get(), -1, 0);

  // If the child requests it, we will manually set the size request for
  // |child_| every time the |widget_| changes sizes. This is mainly useful
  // for bars, where we want the child to expand to fill all available space.
  g_signal_connect(widget_.get(), "child-size-request",
                   G_CALLBACK(OnChildSizeRequest),
                   GINT_TO_POINTER(control_child_size));

  // We connect to this signal to set an initial position for our child widget.
  // The reason we connect to this signal rather than setting the initial
  // position here is that the widget is currently unallocated and may not
  // even have a size request.
  g_signal_connect(child, "size-allocate",
                   G_CALLBACK(OnChildSizeAllocate), this);

  child_needs_move_ = (direction == DOWN);

  animation_.reset(new ui::SlideAnimation(this));
  // Default tween type is EASE_OUT.
  if (linear)
    animation_->SetTweenType(ui::Tween::LINEAR);
  if (duration != 0)
    animation_->SetSlideDuration(duration);
}

SlideAnimatorGtk::~SlideAnimatorGtk() {
  widget_.Destroy();
}

void SlideAnimatorGtk::Open() {
  if (!animations_enabled_)
    return OpenWithoutAnimation();

  gtk_widget_show(widget_.get());
  animation_->Show();
}

void SlideAnimatorGtk::OpenWithoutAnimation() {
  gtk_widget_show(widget_.get());
  animation_->Reset(1.0);
  animation_->Show();
  AnimationProgressed(animation_.get());
}

void SlideAnimatorGtk::Close() {
  if (!animations_enabled_)
    return CloseWithoutAnimation();

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
  return animation_->is_animating();
}

void SlideAnimatorGtk::AnimationProgressed(const ui::Animation* animation) {
  GtkRequisition req;
  gtk_widget_size_request(child_, &req);

  int showing_height = static_cast<int>(req.height *
                                        animation_->GetCurrentValue());
  if (direction_ == DOWN) {
    gtk_expanded_container_move(GTK_EXPANDED_CONTAINER(widget_.get()),
                                child_, 0, showing_height - req.height);
    child_needs_move_ = false;
  }
  gtk_widget_set_size_request(widget_.get(), -1, showing_height);
}

void SlideAnimatorGtk::AnimationEnded(const ui::Animation* animation) {
  if (!animation_->IsShowing()) {
    gtk_widget_hide(widget_.get());
    if (delegate_)
      delegate_->Closed();
  }
}

// static
void SlideAnimatorGtk::SetAnimationsForTesting(bool enable) {
  animations_enabled_ = enable;
}

// static
void SlideAnimatorGtk::OnChildSizeAllocate(GtkWidget* child,
                                           GtkAllocation* allocation,
                                           SlideAnimatorGtk* slider) {
  if (slider->child_needs_move_) {
    gtk_expanded_container_move(GTK_EXPANDED_CONTAINER(slider->widget()),
                                child, 0, -allocation->height);
    slider->child_needs_move_ = false;
  }
}
