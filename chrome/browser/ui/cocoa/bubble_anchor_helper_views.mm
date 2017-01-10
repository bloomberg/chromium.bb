// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Self-deleting object that hosts Objective-C observers watching for parent
// window resizes to reposition a bubble Widget. Deletes itself when the bubble
// Widget closes.
class BubbleAnchorHelper : public views::WidgetObserver {
 public:
  explicit BubbleAnchorHelper(views::BubbleDialogDelegateView* bubble);

 private:
  // Observe |name| on the bubble parent window with a block to call ReAnchor().
  void Observe(NSString* name);

  // Whether offset from the left of the parent window is fixed.
  bool IsMinXFixed() const {
    return views::BubbleBorder::is_arrow_on_left(bubble_->arrow());
  }

  // Re-positions |bubble_| so that the offset to the parent window at
  // construction time is preserved.
  void ReAnchor();

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::scoped_nsobject<NSMutableArray> observer_tokens_;
  views::BubbleDialogDelegateView* bubble_;
  CGFloat horizontal_offset_;  // Offset from the left or right.
  CGFloat vertical_offset_;    // Offset from the top.

  DISALLOW_COPY_AND_ASSIGN(BubbleAnchorHelper);
};

}  // namespace

void KeepBubbleAnchored(views::BubbleDialogDelegateView* bubble) {
  new BubbleAnchorHelper(bubble);
}

BubbleAnchorHelper::BubbleAnchorHelper(views::BubbleDialogDelegateView* bubble)
    : observer_tokens_([[NSMutableArray alloc] init]), bubble_(bubble) {
  DCHECK(bubble->GetWidget());
  DCHECK(bubble->parent_window());
  bubble->GetWidget()->AddObserver(this);

  NSRect parent_frame = [[bubble->parent_window() window] frame];
  NSRect bubble_frame = [bubble->GetWidget()->GetNativeWindow() frame];

  // Note: when anchored on the right, this doesn't support changes to the
  // bubble size, just the parent size.
  horizontal_offset_ =
      (IsMinXFixed() ? NSMinX(parent_frame) : NSMaxX(parent_frame)) -
      NSMinX(bubble_frame);
  vertical_offset_ = NSMaxY(parent_frame) - NSMinY(bubble_frame);

  Observe(NSWindowDidEnterFullScreenNotification);
  Observe(NSWindowDidExitFullScreenNotification);
  Observe(NSWindowDidResizeNotification);

  // Also monitor move. Note that for user-initiated window moves this is not
  // necessary: the bubble's child window status keeps the position pinned to
  // the parent during the move (which is handy since AppKit doesn't send out
  // notifications until the move completes). Programmatic -[NSWindow
  // setFrame:..] calls, however, do not update child window positions for us.
  Observe(NSWindowDidMoveNotification);
}

void BubbleAnchorHelper::Observe(NSString* name) {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  id token = [center addObserverForName:name
                                 object:[bubble_->parent_window() window]
                                  queue:nil
                             usingBlock:^(NSNotification* notification) {
                               ReAnchor();
                             }];
  [observer_tokens_ addObject:token];
}

void BubbleAnchorHelper::ReAnchor() {
  NSRect bubble_frame = [bubble_->GetWidget()->GetNativeWindow() frame];
  NSRect parent_frame = [[bubble_->parent_window() window] frame];
  if (IsMinXFixed())
    bubble_frame.origin.x = NSMinX(parent_frame) - horizontal_offset_;
  else
    bubble_frame.origin.x = NSMaxX(parent_frame) - horizontal_offset_;
  bubble_frame.origin.y = NSMaxY(parent_frame) - vertical_offset_;
  [bubble_->GetWidget()->GetNativeWindow() setFrame:bubble_frame
                                            display:YES
                                            animate:NO];
}

void BubbleAnchorHelper::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  for (id token in observer_tokens_.get())
    [[NSNotificationCenter defaultCenter] removeObserver:token];
  delete this;
}
