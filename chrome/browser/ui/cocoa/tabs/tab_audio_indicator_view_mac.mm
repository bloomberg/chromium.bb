// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_audio_indicator_view_mac.h"

#include "chrome/browser/ui/tabs/tab_audio_indicator.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

class TabAudioIndicatorDelegateMac : public TabAudioIndicator::Delegate {
 public:
  explicit TabAudioIndicatorDelegateMac(TabAudioIndicatorViewMac* view)
      : view_(view) {
  }

  virtual ~TabAudioIndicatorDelegateMac() {}

  virtual void ScheduleAudioIndicatorPaint() OVERRIDE {
    [view_ setNeedsDisplay:YES];
  }

 private:
  TabAudioIndicatorViewMac* view_;

  DISALLOW_COPY_AND_ASSIGN(TabAudioIndicatorDelegateMac);
};

@interface TabAudioIndicatorViewMac ()
@end

@implementation TabAudioIndicatorViewMac

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super initWithFrame:frame])) {
    delegate_.reset(new TabAudioIndicatorDelegateMac(self));
    tabAudioIndicator_.reset(new TabAudioIndicator(delegate_.get()));
  }
  return self;
}

- (void)setIsPlayingAudio:(BOOL)isPlayingAudio {
  tabAudioIndicator_->SetIsPlayingAudio(isPlayingAudio);
}

- (void)setBackgroundImage:(NSImage*)backgroundImage {
  gfx::Image image([backgroundImage retain]);
  tabAudioIndicator_->set_favicon(*image.ToImageSkia());
}

- (BOOL)isAnimating {
  return tabAudioIndicator_->IsAnimating();
}

- (void)drawRect:(NSRect)rect {
  gfx::CanvasSkiaPaint canvas(rect, false);
  canvas.set_composite_alpha(true);
  tabAudioIndicator_->Paint(&canvas, gfx::Rect(NSRectToCGRect([self bounds])));
}

@end
