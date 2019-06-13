// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/color_chooser_mac.h"

#include "base/logging.h"
#include "chrome/browser/ui/color_chooser.h"
#include "skia/ext/skia_utils_mac.h"

namespace {
// The currently active color chooser.
ColorChooserMac* g_current_color_chooser = nullptr;
}  // namespace

// static
ColorChooserMac* ColorChooserMac::Open(content::WebContents* web_contents,
                                       SkColor initial_color) {
  if (g_current_color_chooser)
    g_current_color_chooser->End();
  DCHECK(!g_current_color_chooser);
  // Note that WebContentsImpl::ColorChooser ultimately takes ownership (and
  // deletes) the returned pointer.
  g_current_color_chooser = new ColorChooserMac(web_contents, initial_color);
  return g_current_color_chooser;
}

ColorChooserMac::ColorChooserMac(content::WebContents* web_contents,
                                 SkColor initial_color)
    : web_contents_(web_contents) {
  ColorPanelListener* listener = [ColorPanelListener instance];
  [listener setColor:skia::SkColorToDeviceNSColor(initial_color)];
  [listener showColorPanel];
}

ColorChooserMac::~ColorChooserMac() {
  // Always call End() before destroying.
  DCHECK_NE(g_current_color_chooser, this);
}

void ColorChooserMac::DidChooseColorInColorPanel(SkColor color) {
  DCHECK_EQ(g_current_color_chooser, this);
  if (web_contents_)
    web_contents_->DidChooseColorInColorChooser(color);
}

void ColorChooserMac::DidCloseColorPanel() {
  DCHECK_EQ(g_current_color_chooser, this);
  End();
}

void ColorChooserMac::End() {
  if (g_current_color_chooser == this) {
    g_current_color_chooser = nullptr;
    if (web_contents_)
      web_contents_->DidEndColorChooser();
  }
}

void ColorChooserMac::SetSelectedColor(SkColor color) {
  ColorPanelListener* listener = [ColorPanelListener instance];
  [listener setColor:skia::SkColorToDeviceNSColor(color)];
}

@implementation ColorPanelListener

- (id)init {
  if ((self = [super init])) {
    NSColorPanel* panel = [NSColorPanel sharedColorPanel];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowWillClose:)
               name:NSWindowWillCloseNotification
             object:panel];
  }
  return self;
}

- (void)dealloc {
  // This object is never freed.
  NOTREACHED();
  [super dealloc];
}

- (void)windowWillClose:(NSNotification*)notification {
  if (g_current_color_chooser)
    g_current_color_chooser->DidCloseColorPanel();
  nonUserChange_ = NO;
}

- (void)didChooseColor:(NSColorPanel*)panel {
  if (nonUserChange_) {
    nonUserChange_ = NO;
    return;
  }
  nonUserChange_ = NO;
  NSColor* color = [panel color];
  if ([[color colorSpaceName] isEqualToString:NSNamedColorSpace]) {
    color = [color colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
    // Some colors in "Developer" palette in "Color Palettes" tab can't be
    // converted to RGB. We just ignore such colors.
    // TODO(tkent): We should notice the rejection to users.
    if (!color)
      return;
  }
  SkColor skColor = 0;
  if ([color colorSpace] == [NSColorSpace genericRGBColorSpace]) {
    // genericRGB -> deviceRGB conversion isn't ignorable.  We'd like to use RGB
    // values shown in NSColorPanel UI.
    CGFloat red, green, blue, alpha;
    [color getRed:&red green:&green blue:&blue alpha:&alpha];
    skColor = SkColorSetARGB(
        SkScalarRoundToInt(255.0 * alpha), SkScalarRoundToInt(255.0 * red),
        SkScalarRoundToInt(255.0 * green), SkScalarRoundToInt(255.0 * blue));
  } else {
    skColor = skia::NSDeviceColorToSkColor(
        [[panel color] colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
  }
  if (g_current_color_chooser)
    g_current_color_chooser->DidChooseColorInColorPanel(skColor);
}

+ (ColorPanelListener*)instance {
  static ColorPanelListener* listener = [[ColorPanelListener alloc] init];
  return listener;
}

- (void)showColorPanel {
  NSColorPanel* panel = [NSColorPanel sharedColorPanel];
  [panel setShowsAlpha:NO];
  [panel setTarget:self];
  [panel setAction:@selector(didChooseColor:)];
  [panel makeKeyAndOrderFront:nil];
}

- (void)setColor:(NSColor*)color {
  nonUserChange_ = YES;
  [[NSColorPanel sharedColorPanel] setColor:color];
}

namespace chrome {

content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color) {
  return ColorChooserMac::Open(web_contents, initial_color);
}

}  // namepace chrome

@end
