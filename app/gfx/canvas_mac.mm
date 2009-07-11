// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/gfx/canvas.h"

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/gfx/rect.h"
#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkShader.h"

namespace gfx {

Canvas::Canvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

Canvas::Canvas() : skia::PlatformCanvas() {
}

Canvas::~Canvas() {
}

// static
void Canvas::SizeStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           int *width, int *height, int flags) {
  *width = font.GetStringWidth(text);
  *height = font.height();
}

void Canvas::DrawStringInt(const std::wstring& text, const gfx::Font& font,
                           const SkColor& color, int x, int y, int w, int h,
                           int flags) {
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  CGContextRef context = beginPlatformPaint();
  CGContextSaveGState(context);

  NSColor* ns_color = [NSColor colorWithDeviceRed:SkColorGetR(color) / 255.0
                                            green:SkColorGetG(color) / 255.0
                                             blue:SkColorGetB(color) / 255.0
                                            alpha:1.0];
  NSMutableParagraphStyle *ns_style =
      [[[NSParagraphStyle alloc] init] autorelease];
  if (flags & TEXT_ALIGN_CENTER)
    [ns_style setAlignment:NSCenterTextAlignment];
  // TODO(awalker): Implement the rest of the Canvas text flags

  NSDictionary* attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
          font.nativeFont(), NSFontAttributeName,
          ns_color, NSForegroundColorAttributeName,
          ns_style, NSParagraphStyleAttributeName,
          nil];

  NSAttributedString* ns_string =
      [[[NSAttributedString alloc] initWithString:base::SysWideToNSString(text)
                                        attributes:attributes] autorelease];
  scoped_cftyperef<CTFramesetterRef> framesetter(
      CTFramesetterCreateWithAttributedString(reinterpret_cast<CFAttributedStringRef>(ns_string)));

  CGRect text_bounds = CGRectMake(x, y, w, h);
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathAddRect(path, NULL, text_bounds);

  scoped_cftyperef<CTFrameRef> frame(
      CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL));
  CTFrameDraw(frame, context);
  CGContextRestoreGState(context);
  endPlatformPaint();
}

}  // namespace gfx
