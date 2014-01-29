// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/panels/panel_stack_window_cocoa.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/panels/panel_cocoa.h"
#import "chrome/browser/ui/cocoa/panels/panel_utils_cocoa.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/vector2d.h"
#include "ui/snapshot/snapshot.h"

// The delegate class to receive the notification from NSViewAnimation.
@interface BatchBoundsAnimationDelegate : NSObject<NSAnimationDelegate> {
 @private
  PanelStackWindowCocoa* window_;  // Weak pointer.
}

// Called when NSViewAnimation finishes the animation.
- (void)animationDidEnd:(NSAnimation*)animation;
@end

@implementation BatchBoundsAnimationDelegate

- (id)initWithWindow:(PanelStackWindowCocoa*)window {
  if ((self = [super init]))
    window_ = window;
  return self;
}

- (void)animationDidEnd:(NSAnimation*)animation {
  window_->BoundsUpdateAnimationEnded();
}

@end

// static
NativePanelStackWindow* NativePanelStackWindow::Create(
    NativePanelStackWindowDelegate* delegate) {
  return new PanelStackWindowCocoa(delegate);
}

PanelStackWindowCocoa::PanelStackWindowCocoa(
    NativePanelStackWindowDelegate* delegate)
    : delegate_(delegate),
      attention_request_id_(0),
      bounds_updates_started_(false),
      animate_bounds_updates_(false),
      bounds_animation_(nil) {
  DCHECK(delegate);
  bounds_animation_delegate_.reset(
      [[BatchBoundsAnimationDelegate alloc] initWithWindow:this]);
}

PanelStackWindowCocoa::~PanelStackWindowCocoa() {
}

void PanelStackWindowCocoa::Close() {
  TerminateBoundsAnimation();
  [window_ close];
}

void PanelStackWindowCocoa::AddPanel(Panel* panel) {
  panels_.push_back(panel);

  EnsureWindowCreated();

  // Make the stack window own the panel window such that all panels window
  // could be moved simulatenously when the stack window is moved.
  [window_ addChildWindow:panel->GetNativeWindow() ordered:NSWindowAbove];

  UpdateStackWindowBounds();
}

void PanelStackWindowCocoa::RemovePanel(Panel* panel) {
  if (IsAnimatingPanelBounds()) {
    // This panel is gone. We should not perform any update to it.
    bounds_updates_.erase(panel);
  }

  panels_.remove(panel);

  // If the native panel is closed, the native window should already be gone.
  if (!static_cast<PanelCocoa*>(panel->native_panel())->IsClosed())
    [window_ removeChildWindow:panel->GetNativeWindow()];

  UpdateStackWindowBounds();
}

void PanelStackWindowCocoa::MergeWith(NativePanelStackWindow* another) {
  PanelStackWindowCocoa* another_stack =
      static_cast<PanelStackWindowCocoa*>(another);

  for (Panels::const_iterator iter = another_stack->panels_.begin();
       iter != another_stack->panels_.end(); ++iter) {
    Panel* panel = *iter;
    panels_.push_back(panel);

    // Change the panel window owner.
    NSWindow* panel_window = panel->GetNativeWindow();
    [another_stack->window_ removeChildWindow:panel_window];
    [window_ addChildWindow:panel_window ordered:NSWindowAbove];
  }
  another_stack->panels_.clear();

  UpdateStackWindowBounds();
}

bool PanelStackWindowCocoa::IsEmpty() const {
  return panels_.empty();
}

bool PanelStackWindowCocoa::HasPanel(Panel* panel) const {
  return std::find(panels_.begin(), panels_.end(), panel) != panels_.end();
}

void PanelStackWindowCocoa::MovePanelsBy(const gfx::Vector2d& delta) {
  // Moving the background stack window will cause all foreground panels window
  // being moved simulatenously.
  gfx::Rect enclosing_bounds = GetStackWindowBounds();
  enclosing_bounds.Offset(delta);
  NSRect frame = cocoa_utils::ConvertRectToCocoaCoordinates(enclosing_bounds);
  [window_ setFrame:frame display:NO];

  // We also need to update the panel bounds.
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    gfx::Rect bounds = panel->GetBounds();
    bounds.Offset(delta);
    panel->SetPanelBoundsInstantly(bounds);
  }
}

void PanelStackWindowCocoa::BeginBatchUpdatePanelBounds(bool animate) {
  // If the batch animation is still in progress, continue the animation
  // with the new target bounds even we want to update the bounds instantly
  // this time.
  if (!bounds_updates_started_) {
    animate_bounds_updates_ = animate;
    bounds_updates_started_ = true;
  }
}

void PanelStackWindowCocoa::AddPanelBoundsForBatchUpdate(
    Panel* panel, const gfx::Rect& new_bounds) {
  DCHECK(bounds_updates_started_);

  // No need to track it if no change is needed.
  if (panel->GetBounds() == new_bounds)
    return;

  // Old bounds are stored as the map value.
  bounds_updates_[panel] = panel->GetBounds();

  // New bounds are directly applied to the value stored in native panel
  // window.
  static_cast<PanelCocoa*>(panel->native_panel())->set_cached_bounds_directly(
      new_bounds);
}

void PanelStackWindowCocoa::EndBatchUpdatePanelBounds() {
  DCHECK(bounds_updates_started_);

  // No need to proceed with the animation when the bounds update list is
  // empty or animation was not requested.
  if (bounds_updates_.empty() || !animate_bounds_updates_) {
    // Set the bounds directly when the update list is not empty.
    if (!bounds_updates_.empty()) {
      for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
           iter != bounds_updates_.end(); ++iter) {
        Panel* panel = iter->first;
        NSRect frame =
            cocoa_utils::ConvertRectToCocoaCoordinates(panel->GetBounds());
        [panel->GetNativeWindow() setFrame:frame display:YES animate:NO];
      }
      bounds_updates_.clear();
      UpdateStackWindowBounds();
    }

    bounds_updates_started_ = false;
    delegate_->PanelBoundsBatchUpdateCompleted();
    return;
  }

  // Terminate previous animation, if it is still playing.
  TerminateBoundsAnimation();

  // Find out if we need the animation for each panel. If the batch updates
  // consist of only moving all panels by delta offset, moving the background
  // window would be enough.

  // If all the panels move and don't resize, just animate the underlying
  // parent window. Otherwise, animate each individual panel.
  bool need_to_animate_individual_panels = false;
  if (bounds_updates_.size() == panels_.size()) {
    gfx::Vector2d delta;
    for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
         iter != bounds_updates_.end(); ++iter) {
      gfx::Rect old_bounds = iter->second;
      gfx::Rect new_bounds = iter->first->GetBounds();

      // Size should not be changed.
      if (old_bounds.width() != new_bounds.width() ||
          old_bounds.height() != new_bounds.height()) {
        need_to_animate_individual_panels = true;
        break;
      }

      // Origin offset should be same.
      if (iter == bounds_updates_.begin()) {
        delta = new_bounds.origin() - old_bounds.origin();
      } else if (!(delta == new_bounds.origin() - old_bounds.origin())) {
        need_to_animate_individual_panels = true;
        break;
      }
    }
  } else {
    need_to_animate_individual_panels = true;
  }

  int num_of_animations = 1;
  if (need_to_animate_individual_panels)
    num_of_animations += bounds_updates_.size();
  base::scoped_nsobject<NSMutableArray> animations(
      [[NSMutableArray alloc] initWithCapacity:num_of_animations]);

  // Add the animation for each panel in the update list.
  if (need_to_animate_individual_panels) {
    for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
         iter != bounds_updates_.end(); ++iter) {
      Panel* panel = iter->first;
      NSRect panel_frame =
          cocoa_utils::ConvertRectToCocoaCoordinates(panel->GetBounds());
      NSDictionary* animation = [NSDictionary dictionaryWithObjectsAndKeys:
          panel->GetNativeWindow(), NSViewAnimationTargetKey,
          [NSValue valueWithRect:panel_frame], NSViewAnimationEndFrameKey,
          nil];
      [animations addObject:animation];
    }
  }

  // Compute the final bounds that enclose all panels after the animation.
  gfx::Rect enclosing_bounds;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    gfx::Rect target_bounds = (*iter)->GetBounds();
    enclosing_bounds = UnionRects(enclosing_bounds, target_bounds);
  }

  // Add the animation for the background stack window.
  NSRect enclosing_frame =
      cocoa_utils::ConvertRectToCocoaCoordinates(enclosing_bounds);
  NSDictionary* stack_animation = [NSDictionary dictionaryWithObjectsAndKeys:
      window_.get(), NSViewAnimationTargetKey,
      [NSValue valueWithRect:enclosing_frame], NSViewAnimationEndFrameKey,
      nil];
  [animations addObject:stack_animation];

  // Start all the animations.
  // |bounds_animation_| is released when the animation ends.
  bounds_animation_ =
      [[NSViewAnimation alloc] initWithViewAnimations:animations];
  [bounds_animation_ setDelegate:bounds_animation_delegate_.get()];
  [bounds_animation_ setDuration:PanelManager::AdjustTimeInterval(0.18)];
  [bounds_animation_ setFrameRate:0.0];
  [bounds_animation_ setAnimationBlockingMode: NSAnimationNonblocking];
  [bounds_animation_ startAnimation];
}

bool PanelStackWindowCocoa::IsAnimatingPanelBounds() const {
  return bounds_updates_started_ && animate_bounds_updates_;
}

void PanelStackWindowCocoa::BoundsUpdateAnimationEnded() {
  bounds_updates_started_ = false;

  for (BoundsUpdates::const_iterator iter = bounds_updates_.begin();
       iter != bounds_updates_.end(); ++iter) {
    Panel* panel = iter->first;
    panel->manager()->OnPanelAnimationEnded(panel);
  }
  bounds_updates_.clear();

  delegate_->PanelBoundsBatchUpdateCompleted();
}

void PanelStackWindowCocoa::Minimize() {
  // Provide the custom miniwindow image since there is nothing painted for
  // the background stack window.
  gfx::Size stack_window_size = GetStackWindowBounds().size();
  gfx::Canvas canvas(stack_window_size, 1.0f, true);
  int y = 0;
  Panels::const_iterator iter = panels_.begin();
  for (; iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    gfx::Rect snapshot_bounds = gfx::Rect(panel->GetBounds().size());
    std::vector<unsigned char> png;
    if (!ui::GrabWindowSnapshot(panel->GetNativeWindow(),
                                &png,
                                snapshot_bounds))
      break;
    gfx::Image snapshot_image = gfx::Image::CreateFrom1xPNGBytes(
        &(png[0]), png.size());
    canvas.DrawImageInt(snapshot_image.AsImageSkia(), 0, y);
    y += snapshot_bounds.height();
  }
  if (iter == panels_.end()) {
    gfx::Image image(gfx::ImageSkia(canvas.ExtractImageRep()));
    [window_ setMiniwindowImage:image.AsNSImage()];
  }

  [window_ miniaturize:nil];
}

bool PanelStackWindowCocoa::IsMinimized() const {
  return [window_ isMiniaturized];
}

void PanelStackWindowCocoa::DrawSystemAttention(bool draw_attention) {
  BOOL is_drawing_attention = attention_request_id_ != 0;
  if (draw_attention == is_drawing_attention)
    return;

  if (draw_attention) {
    attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

void PanelStackWindowCocoa::OnPanelActivated(Panel* panel) {
  // Nothing to do.
}

void PanelStackWindowCocoa::TerminateBoundsAnimation() {
  if (!bounds_animation_)
    return;
  [bounds_animation_ stopAnimation];
  [bounds_animation_ setDelegate:nil];
  [bounds_animation_ release];
  bounds_animation_ = nil;
}

gfx::Rect PanelStackWindowCocoa::GetStackWindowBounds() const {
  gfx::Rect enclosing_bounds;
  for (Panels::const_iterator iter = panels_.begin();
       iter != panels_.end(); ++iter) {
    Panel* panel = *iter;
    enclosing_bounds = UnionRects(enclosing_bounds, panel->GetBounds());
  }
  return enclosing_bounds;
}

void PanelStackWindowCocoa::UpdateStackWindowBounds() {
  NSRect enclosing_bounds =
      cocoa_utils::ConvertRectToCocoaCoordinates(GetStackWindowBounds());
  [window_ setFrame:enclosing_bounds display:NO];
}

void PanelStackWindowCocoa::EnsureWindowCreated() {
  if (window_)
    return;

  window_.reset(
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setHasShadow:YES];
  [window_ setLevel:NSNormalWindowLevel];
  [window_ orderFront:nil];
  [window_ setTitle:base::SysUTF16ToNSString(delegate_->GetTitle())];
}
