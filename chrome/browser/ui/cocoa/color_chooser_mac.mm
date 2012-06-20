// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/color_chooser.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#import "base/mac/cocoa_protocols.h"
#import "base/memory/scoped_nsobject.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "skia/ext/skia_utils_mac.h"

class ColorChooserMac;

// A Listener class to act as a event target for NSColorPanel and send
// the results to the C++ class, ColorChooserMac.
@interface ColorPanelCocoa : NSObject<NSWindowDelegate> {
 @private
  // We don't call DidChooseColor if the change wasn't caused by the user
  // interacting with the panel.
  BOOL nonUserChange_;
  ColorChooserMac* chooser_;  // weak, owns this
}

- (id)initWithChooser:(ColorChooserMac*)chooser;

// Called from NSColorPanel.
- (void)didChooseColor:(NSColorPanel*)panel;

// Sets color to the NSColorPanel as a non user change.
- (void)setColor:(NSColor*)color;

@end

class ColorChooserMac : public content::ColorChooser,
                        public content::WebContentsObserver {
 public:
  ColorChooserMac(
      int identifier, content::WebContents* tab, SkColor initial_color);
  virtual ~ColorChooserMac();

  // Called from ColorPanelCocoa.
  void DidChooseColor(SkColor color);
  void DidClose();

  virtual void End() OVERRIDE;
  virtual void SetSelectedColor(SkColor color) OVERRIDE;

 private:
  scoped_nsobject<ColorPanelCocoa> panel_;
};

content::ColorChooser* content::ColorChooser::Create(
    int identifier, content::WebContents* tab, SkColor initial_color) {
  return new ColorChooserMac(identifier, tab, initial_color);
}

ColorChooserMac::ColorChooserMac(
    int identifier, content::WebContents* tab, SkColor initial_color)
    : content::ColorChooser(identifier),
      content::WebContentsObserver(tab) {
  panel_.reset([[ColorPanelCocoa alloc] initWithChooser:this]);
  [panel_ setColor:gfx::SkColorToDeviceNSColor(initial_color)];
  [[NSColorPanel sharedColorPanel] makeKeyAndOrderFront:nil];
}

ColorChooserMac::~ColorChooserMac() {
  // Always call End() before destroying.
  DCHECK(!panel_);
}

void ColorChooserMac::DidChooseColor(SkColor color) {
  if (web_contents())
    web_contents()->DidChooseColorInColorChooser(identifier(), color);
}

void ColorChooserMac::DidClose() {
  End();
}

void ColorChooserMac::End() {
  panel_.reset();
  if (web_contents())
    web_contents()->DidEndColorChooser(identifier());
}

void ColorChooserMac::SetSelectedColor(SkColor color) {
  [panel_ setColor:gfx::SkColorToDeviceNSColor(color)];
}

@implementation ColorPanelCocoa

- (id)initWithChooser:(ColorChooserMac*)chooser {
  if ((self = [super init])) {
    chooser_ = chooser;
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    [panel setShowsAlpha:NO];
    [panel setDelegate:self];
    [panel setTarget:self];
    [panel setAction:@selector(didChooseColor:)];
  }
  return self;
}

- (void)dealloc {
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  if ([panel delegate] == self) {
    [panel setDelegate:nil];
    [panel setTarget:nil];
    [panel setAction:nil];
  }

  [super dealloc];
}

- (void)windowWillClose:(NSNotification*)notification {
  chooser_->DidClose();
  nonUserChange_ = NO;
}

- (void)didChooseColor:(NSColorPanel*)panel {
  if (nonUserChange_) {
    nonUserChange_ = NO;
    return;
  }
  chooser_->DidChooseColor(gfx::NSDeviceColorToSkColor(
      [[panel color] colorUsingColorSpaceName:NSDeviceRGBColorSpace]));
  nonUserChange_ = NO;
}

- (void)setColor:(NSColor*)color {
  nonUserChange_ = YES;
  [[NSColorPanel sharedColorPanel] setColor:color];
}

@end
