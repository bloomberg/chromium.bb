// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/confirm_bubble.h"

#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#import "chrome/browser/ui/cocoa/confirm_bubble_controller.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/views/confirm_bubble_views.h"
#include "components/constrained_window/constrained_window_views.h"

namespace {

void ShowConfirmBubbleViews(gfx::NativeWindow window,
                            std::unique_ptr<ConfirmBubbleModel> model) {
  constrained_window::CreateBrowserModalDialogViews(
      new ConfirmBubbleViews(std::move(model)), window)
      ->Show();
}

}  // namespace

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       std::unique_ptr<ConfirmBubbleModel> model) {
  if (chrome::ShowAllDialogsWithViewsToolkit()) {
    ShowConfirmBubbleViews(window, std::move(model));
    return;
  }

  // Create a custom NSViewController that manages a bubble view, and add it to
  // a child to the specified |anchor_view|. This controller will be
  // automatically deleted when it loses first-responder status.
  ConfirmBubbleController* controller =
      [[ConfirmBubbleController alloc] initWithParent:anchor_view
                                               origin:origin.ToCGPoint()
                                                model:std::move(model)];
  [anchor_view addSubview:[controller view]
               positioned:NSWindowAbove
               relativeTo:nil];
  [[anchor_view window] makeFirstResponder:[controller view]];
}

}  // namespace chrome
