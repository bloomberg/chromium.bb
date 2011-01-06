// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>

@interface BalloonWindow : NSWindow {
}
@end

// This view class draws a frame around the HTML contents of a
// notification balloon.
@interface BalloonContentViewCocoa : NSView {
}
@end

// This view class draws the shelf of a notification balloon,
// containing the controls.
@interface BalloonShelfViewCocoa : NSView {
}
@end

// This view overlays the notification balloon on top. It is used to intercept
// mouse input to prevent reordering of the other browser windows when clicking
// on the notification balloon.
@interface BalloonOverlayViewCocoa : NSView {
}
@end


#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_BALLOON_VIEW_H_
