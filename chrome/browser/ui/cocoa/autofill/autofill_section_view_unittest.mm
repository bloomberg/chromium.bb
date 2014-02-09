// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_section_view.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillSectionViewTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    view_.reset(
        [[AutofillSectionView alloc] initWithFrame:NSMakeRect(0, 0, 20, 20)]);
    [[test_window() contentView] addSubview:view_];
  }

  NSEvent* fakeEvent(NSEventType type) {
    return [NSEvent enterExitEventWithType:type
                              location:NSZeroPoint
                         modifierFlags:0
                             timestamp:0
                          windowNumber:[[view_ window] windowNumber]
                               context:[NSGraphicsContext currentContext]
                           eventNumber:0
                        trackingNumber:0
                              userData:nil];
  }

  void ResetDrawingContext(NSColor* fillColor) {
    NSRect bounds = [view_ bounds];
    if (!bitmap_) {
      bitmap_.reset(
          [[NSBitmapImageRep alloc]
              initWithBitmapDataPlanes: NULL
                            pixelsWide: NSWidth(bounds)
                            pixelsHigh: NSHeight(bounds)
                         bitsPerSample: 8
                       samplesPerPixel: 4
                              hasAlpha: YES
                              isPlanar: NO
                        colorSpaceName: NSCalibratedRGBColorSpace
                           bytesPerRow: NSWidth(bounds) * 4
                          bitsPerPixel: 32]);
    }

    if (!saved_context_) {
      saved_context_.reset(new gfx::ScopedNSGraphicsContextSaveGState);
      context_ = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap_];
      [NSGraphicsContext setCurrentContext:context_];
    }

    [fillColor set];
    NSRectFill(bounds);
  }

  void CheckImageIsSolidColor(NSColor* color) {
    ASSERT_TRUE(saved_context_);
    [context_ flushGraphics];
    NSRect bounds = [view_ bounds];
    for (NSInteger y = 0; y < NSHeight(bounds); ++y) {
      for (NSInteger x = 0; x < NSWidth(bounds); ++x) {
        ASSERT_NSEQ(color, [bitmap_ colorAtX:x y:y]);
      }
    }
  }

 protected:
  base::scoped_nsobject<AutofillSectionView> view_;
  base::scoped_nsobject<NSBitmapImageRep> bitmap_;
  scoped_ptr<gfx::ScopedNSGraphicsContextSaveGState> saved_context_;
  NSGraphicsContext* context_;
};

}  // namespace

// A simple delegate to intercept and register performClick: invocation.
@interface AutofillClickTestDelegate : NSControl {
 @private
  BOOL didFire_;
}
@property(readonly, nonatomic) BOOL didFire;
@end


@implementation AutofillClickTestDelegate

@synthesize didFire = didFire_;

// Override performClick to record invocation.
- (void)performClick:(id)sender {
  didFire_ = YES;
}

@end

TEST_VIEW(AutofillSectionViewTest, view_)

TEST_F(AutofillSectionViewTest, HoverColorIsOpaque) {
  EXPECT_EQ(1.0, [[view_ hoverColor] alphaComponent]);
}

TEST_F(AutofillSectionViewTest, EventsCauseHighlighting) {
  EXPECT_FALSE([view_ isHighlighted]);

  [view_ mouseEntered:fakeEvent(NSMouseEntered)];
  EXPECT_TRUE([view_ isHighlighted]);

  [view_ mouseExited:fakeEvent(NSMouseExited)];
  EXPECT_FALSE([view_ isHighlighted]);
}

TEST_F(AutofillSectionViewTest, HoverHighlighting) {
  NSRect bounds = [view_ bounds];
  NSColor* fillColor = [NSColor blueColor];
  EXPECT_NSNE(fillColor, [view_ hoverColor]);

  [view_ setShouldHighlightOnHover:YES];

  // Adjust hoverColor for render quantization effects.
  ResetDrawingContext([view_ hoverColor]);
  [context_ flushGraphics];
  NSColor* quantizedHoverColor = [bitmap_ colorAtX:0 y:0];

  // Test a highlighted view has the right color.
  ResetDrawingContext(fillColor);
  [view_ mouseEntered:fakeEvent(NSMouseEntered)];
  [view_ drawRect:bounds];
  CheckImageIsSolidColor(quantizedHoverColor);

  // Test a non-highlighted view doesn't change existing background.
  ResetDrawingContext(fillColor);
  [view_ mouseEntered:fakeEvent(NSMouseExited)];
  [view_ drawRect:bounds];
  CheckImageIsSolidColor(fillColor);

  // Test there is no higlight if highlight mode is off.
  [view_ setShouldHighlightOnHover:NO];
  ResetDrawingContext(fillColor);
  [view_ mouseEntered:fakeEvent(NSMouseEntered)];
  [view_ drawRect:bounds];
  CheckImageIsSolidColor(fillColor);
}

TEST_F(AutofillSectionViewTest, MouseClicksAreForwarded) {
  base::scoped_nsobject<AutofillClickTestDelegate> delegate(
      [[AutofillClickTestDelegate alloc] init]);
  [view_ setClickTarget:delegate];

  NSEvent* down_event =
      cocoa_test_event_utils::LeftMouseDownAtPoint(NSMakePoint(5, 5));
  ASSERT_FALSE([delegate didFire]);
  [test_window() sendEvent:down_event];
  EXPECT_TRUE([delegate didFire]);
}
