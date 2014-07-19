// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_web_theme_engine.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkRect.h"

using blink::WebCanvas;
using blink::WebColor;
using blink::WebRect;
using blink::WebThemeEngine;

namespace content {

namespace {

const SkColor edgeColor = SK_ColorBLACK;
const SkColor readOnlyColor = SkColorSetRGB(0xe9, 0xc2, 0xa6);

}  // namespace

SkColor bgColors(WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::StateDisabled:
      return SkColorSetRGB(0xc9, 0xc9, 0xc9);
    case WebThemeEngine::StateHover:
      return SkColorSetRGB(0x43, 0xf9, 0xff);
    case WebThemeEngine::StateNormal:
      return SkColorSetRGB(0x89, 0xc4, 0xff);
    case WebThemeEngine::StatePressed:
      return SkColorSetRGB(0xa9, 0xff, 0x12);
    case WebThemeEngine::StateFocused:
      return SkColorSetRGB(0x00, 0xf3, 0xac);
    case WebThemeEngine::StateReadonly:
      return SkColorSetRGB(0xf3, 0xe0, 0xd0);
    default:
      NOTREACHED();
  }
  return SkColorSetRGB(0x00, 0x00, 0xff);
}

blink::WebSize MockWebThemeEngine::getSize(WebThemeEngine::Part part) {
    // FIXME: We use this constant to indicate we are being asked for the size of
    // a part that we don't expect to be asked about. We return a garbage value
    // rather than just asserting because this code doesn't have access to either
    // WTF or base to raise an assertion or do any logging :(.
    const blink::WebSize invalidPartSize = blink::WebSize(100, 100);

    switch (part) {
    case WebThemeEngine::PartScrollbarLeftArrow:
        return blink::WebSize(17, 15);
    case WebThemeEngine::PartScrollbarRightArrow:
        return invalidPartSize;
    case WebThemeEngine::PartScrollbarUpArrow:
        return blink::WebSize(15, 17);
    case WebThemeEngine::PartScrollbarDownArrow:
        return invalidPartSize;
    case WebThemeEngine::PartScrollbarHorizontalThumb:
        return blink::WebSize(15, 15);
    case WebThemeEngine::PartScrollbarVerticalThumb:
        return blink::WebSize(15, 15);
    case WebThemeEngine::PartScrollbarHorizontalTrack:
        return blink::WebSize(0, 15);
    case WebThemeEngine::PartScrollbarVerticalTrack:
        return blink::WebSize(15, 0);
    case WebThemeEngine::PartCheckbox:
    case WebThemeEngine::PartRadio:
        return blink::WebSize(13, 13);
    case WebThemeEngine::PartSliderThumb:
        return blink::WebSize(11, 21);
    case WebThemeEngine::PartInnerSpinButton:
        return blink::WebSize(15, 8);
    default:
        return invalidPartSize;
    }
}

static SkIRect webRectToSkIRect(const WebRect& webRect) {
    SkIRect irect;
    irect.set(webRect.x, webRect.y,
        webRect.x + webRect.width - 1, webRect.y + webRect.height - 1);
    return irect;
}

static SkIRect validate(const SkIRect& rect, WebThemeEngine::Part part) {
    switch (part) {
    case WebThemeEngine::PartCheckbox:
    case WebThemeEngine::PartRadio: {
        SkIRect retval = rect;

        // The maximum width and height is 13.
        // Center the square in the passed rectangle.
        const int maxControlSize = 13;
        int controlSize = std::min(rect.width(), rect.height());
        controlSize = std::min(controlSize, maxControlSize);

        retval.fLeft   = rect.fLeft + (rect.width() / 2) - (controlSize / 2);
        retval.fRight  = retval.fLeft + controlSize - 1;
        retval.fTop    = rect.fTop + (rect.height() / 2) - (controlSize / 2);
        retval.fBottom = retval.fTop + controlSize - 1;

        return retval;
    }
    default:
        return rect;
    }
}

void box(SkCanvas* canvas, const SkIRect& rect, SkColor fillColor) {
    SkPaint paint;

    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(fillColor);
    canvas->drawIRect(rect, paint);

    paint.setColor(edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawIRect(rect, paint);
}

void line(SkCanvas* canvas, int x0, int y0, int x1, int y1, SkColor color) {
    SkPaint paint;
    paint.setColor(color);
    canvas->drawLine(SkIntToScalar(x0),
                     SkIntToScalar(y0),
                     SkIntToScalar(x1),
                     SkIntToScalar(y1),
                     paint);
}

void triangle(SkCanvas* canvas,
              int x0,
              int y0,
              int x1,
              int y1,
              int x2,
              int y2,
              SkColor color) {
    SkPath path;
    SkPaint paint;

    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    path.incReserve(4);
    path.moveTo(SkIntToScalar(x0), SkIntToScalar(y0));
    path.lineTo(SkIntToScalar(x1), SkIntToScalar(y1));
    path.lineTo(SkIntToScalar(x2), SkIntToScalar(y2));
    path.close();
    canvas->drawPath(path, paint);

    paint.setColor(edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawPath(path, paint);
}

void roundRect(SkCanvas* canvas, SkIRect irect, SkColor color) {
    SkRect rect;
    SkScalar radius = SkIntToScalar(5);
    SkPaint paint;

    rect.set(irect);
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawRoundRect(rect, radius, radius, paint);

    paint.setColor(edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawRoundRect(rect, radius, radius, paint);
}

void oval(SkCanvas* canvas, SkIRect irect, SkColor color) {
    SkRect rect;
    SkPaint paint;

    rect.set(irect);
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawOval(rect, paint);

    paint.setColor(edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawOval(rect, paint);
}

void circle(SkCanvas* canvas, SkIRect irect, SkScalar radius, SkColor color) {
    int left = irect.fLeft;
    int width = irect.width();
    int height = irect.height();
    int top = irect.fTop;

    SkScalar cy = SkIntToScalar(top  + height / 2);
    SkScalar cx = SkIntToScalar(left + width / 2);
    SkPaint paint;

    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawCircle(cx, cy, radius, paint);

    paint.setColor(edgeColor);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawCircle(cx, cy, radius, paint);
}

void nestedBoxes(SkCanvas* canvas,
                 SkIRect irect,
                 int indentLeft,
                 int indentTop,
                 int indentRight,
                 int indentBottom,
                 SkColor outerColor,
                 SkColor innerColor) {
    SkIRect lirect;
    box(canvas, irect, outerColor);
    lirect.set(irect.fLeft + indentLeft,
        irect.fTop + indentTop,
        irect.fRight - indentRight,
        irect.fBottom - indentBottom);
    box(canvas, lirect, innerColor);
}

void insetBox(SkCanvas* canvas,
              SkIRect irect,
              int indentLeft,
              int indentTop,
              int indentRight,
              int indentBottom,
              SkColor color) {
  SkIRect lirect;
  lirect.set(irect.fLeft + indentLeft,
             irect.fTop + indentTop,
             irect.fRight - indentRight,
             irect.fBottom - indentBottom);
  box(canvas, lirect, color);
}

void markState(SkCanvas* canvas, SkIRect irect, WebThemeEngine::State state) {
    int left = irect.fLeft;
    int right = irect.fRight;
    int top = irect.fTop;
    int bottom = irect.fBottom;

    // The length of a triangle side for the corner marks.
    const int triangleSize = 5;

    switch (state) {
    case WebThemeEngine::StateDisabled:
    case WebThemeEngine::StateNormal:
        // Don't visually mark these states (color is enough).
        break;

    case WebThemeEngine::StateReadonly: {
        // The horizontal lines in a read only control are spaced by this amount.
        const int readOnlyLineOffset = 5;

        // Drawing lines across the control.
        for (int i = top + readOnlyLineOffset; i < bottom; i += readOnlyLineOffset)
            line(canvas, left + 1, i, right - 1, i, readOnlyColor);
        break;
    }
    case WebThemeEngine::StateHover:
        // Draw a triangle in the upper left corner of the control. (Win's "hot")
        triangle(canvas,
            left,                 top,
            left + triangleSize,  top,
            left,                 top + triangleSize,
            edgeColor);
        break;

    case WebThemeEngine::StateFocused:
        // Draw a triangle in the bottom right corner of the control.
        triangle(canvas,
            right,                bottom,
            right - triangleSize, bottom,
            right,                bottom - triangleSize,
            edgeColor);
        break;

    case WebThemeEngine::StatePressed:
        // Draw a triangle in the bottom left corner of the control.
        triangle(canvas,
            left,                 bottom,
            left,                 bottom - triangleSize,
            left + triangleSize,  bottom,
            edgeColor);
        break;

    default:
        // FIXME: Should we do something here to indicate that we got an invalid state?
        // Unfortunately, we can't assert because we don't have access to WTF or base.
        break;
    }
}

void MockWebThemeEngine::paint(blink::WebCanvas* canvas,
                               WebThemeEngine::Part part,
                               WebThemeEngine::State state,
                               const blink::WebRect& rect,
                               const WebThemeEngine::ExtraParams* extraParams) {
    SkIRect irect = webRectToSkIRect(rect);
    SkPaint paint;

    // Indent amounts for the check in a checkbox or radio button.
    const int checkIndent = 3;

    // Indent amounts for short and long sides of the scrollbar notches.
    const int notchLongOffset = 1;
    const int notchShortOffset = 4;
    const int noOffset = 0;

    // Indent amounts for the short and long sides of a scroll thumb box.
    const int thumbLongIndent = 0;
    const int thumbShortIndent = 2;

    // Indents for the crosshatch on a scroll grip.
    const int gripLongIndent = 3;
    const int gripShortIndent = 5;

    // Indents for the the slider track.
    const int sliderIndent = 2;

    int halfHeight = irect.height() / 2;
    int halfWidth = irect.width() / 2;
    int quarterHeight = irect.height() / 4;
    int quarterWidth = irect.width() / 4;
    int left = irect.fLeft;
    int right = irect.fRight;
    int top = irect.fTop;
    int bottom = irect.fBottom;

    switch (part) {
    case WebThemeEngine::PartScrollbarDownArrow:
        box(canvas, irect, bgColors(state));
        triangle(canvas,
            left  + quarterWidth, top    + quarterHeight,
            right - quarterWidth, top    + quarterHeight,
            left  + halfWidth,    bottom - quarterHeight,
            edgeColor);
        markState(canvas, irect, state);
        break;

    case WebThemeEngine::PartScrollbarLeftArrow:
        box(canvas, irect, bgColors(state));
        triangle(canvas,
            right - quarterWidth, top    + quarterHeight,
            right - quarterWidth, bottom - quarterHeight,
            left  + quarterWidth, top    + halfHeight,
            edgeColor);
        break;

    case WebThemeEngine::PartScrollbarRightArrow:
        box(canvas, irect, bgColors(state));
        triangle(canvas,
            left  + quarterWidth, top    + quarterHeight,
            right - quarterWidth, top    + halfHeight,
            left  + quarterWidth, bottom - quarterHeight,
            edgeColor);
        break;

    case WebThemeEngine::PartScrollbarUpArrow:
        box(canvas, irect, bgColors(state));
        triangle(canvas,
            left  + quarterWidth, bottom - quarterHeight,
            left  + halfWidth,    top    + quarterHeight,
            right - quarterWidth, bottom - quarterHeight,
            edgeColor);
        markState(canvas, irect, state);
        break;

    case WebThemeEngine::PartScrollbarHorizontalThumb: {
        // Draw a narrower box on top of the outside box.
        nestedBoxes(canvas, irect, thumbLongIndent, thumbShortIndent,
            thumbLongIndent, thumbShortIndent,
            bgColors(state), bgColors(state));
        // Draw a horizontal crosshatch for the grip.
        int longOffset = halfWidth - gripLongIndent;
        line(canvas,
            left  + gripLongIndent, top    + halfHeight,
            right - gripLongIndent, top    + halfHeight,
            edgeColor);
        line(canvas,
            left  + longOffset,     top    + gripShortIndent,
            left  + longOffset,     bottom - gripShortIndent,
            edgeColor);
        line(canvas,
            right - longOffset,     top    + gripShortIndent,
            right - longOffset,     bottom - gripShortIndent,
            edgeColor);
        markState(canvas, irect, state);
        break;
    }

    case WebThemeEngine::PartScrollbarVerticalThumb: {
        // Draw a shorter box on top of the outside box.
        nestedBoxes(canvas, irect, thumbShortIndent, thumbLongIndent,
            thumbShortIndent, thumbLongIndent,
            bgColors(state), bgColors(state));
        // Draw a vertical crosshatch for the grip.
        int longOffset = halfHeight - gripLongIndent;
        line(canvas,
            left  + halfWidth,       top    + gripLongIndent,
            left  + halfWidth,       bottom - gripLongIndent,
            edgeColor);
        line(canvas,
            left  + gripShortIndent, top    + longOffset,
            right - gripShortIndent, top    + longOffset,
            edgeColor);
        line(canvas,
            left  + gripShortIndent, bottom - longOffset,
            right - gripShortIndent, bottom - longOffset,
            edgeColor);
        markState(canvas, irect, state);
        break;
    }

    case WebThemeEngine::PartScrollbarHorizontalTrack: {
        int longOffset = halfHeight - notchLongOffset;
        int shortOffset = irect.width() - notchShortOffset;
        box(canvas, irect, bgColors(state));
        // back, notch on right
        insetBox(canvas,
                 irect,
                 noOffset,
                 longOffset,
                 shortOffset,
                 longOffset,
                 edgeColor);
        // forward, notch on right
        insetBox(canvas,
                 irect,
                 shortOffset,
                 longOffset,
                 noOffset,
                 longOffset,
                 edgeColor);
        markState(canvas, irect, state);
        break;
    }

    case WebThemeEngine::PartScrollbarVerticalTrack: {
        int longOffset = halfWidth - notchLongOffset;
        int shortOffset = irect.height() - notchShortOffset;
        box(canvas, irect, bgColors(state));
        // back, notch at top
        insetBox(canvas,
                 irect,
                 longOffset,
                 noOffset,
                 longOffset,
                 shortOffset,
                 edgeColor);
        // forward, notch at bottom
        insetBox(canvas,
                 irect,
                 longOffset,
                 shortOffset,
                 longOffset,
                 noOffset,
                 edgeColor);
        markState(canvas, irect, state);
        break;
    }

    case WebThemeEngine::PartScrollbarCorner: {
        SkIRect cornerRect = {rect.x, rect.y, rect.x + rect.width, rect.y + rect.height};
        paint.setColor(SK_ColorWHITE);
        paint.setStyle(SkPaint::kFill_Style);
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        paint.setAntiAlias(true);
        canvas->drawIRect(cornerRect, paint);
        break;
    }

    case WebThemeEngine::PartCheckbox:
        if (extraParams->button.indeterminate) {
            nestedBoxes(canvas, irect,
                checkIndent, halfHeight,
                checkIndent, halfHeight,
                bgColors(state), edgeColor);
        } else if (extraParams->button.checked) {
            irect = validate(irect, part);
            nestedBoxes(canvas, irect,
                checkIndent, checkIndent,
                checkIndent, checkIndent,
                bgColors(state), edgeColor);
        } else {
            irect = validate(irect, part);
            box(canvas, irect, bgColors(state));
        }
        break;

    case WebThemeEngine::PartRadio:
        irect = validate(irect, part);
        halfHeight = irect.height() / 2;
        if (extraParams->button.checked) {
            circle(canvas, irect, SkIntToScalar(halfHeight), bgColors(state));
            circle(canvas, irect, SkIntToScalar(halfHeight - checkIndent), edgeColor);
        } else {
            circle(canvas, irect, SkIntToScalar(halfHeight), bgColors(state));
        }
        break;

    case WebThemeEngine::PartButton:
        roundRect(canvas, irect, bgColors(state));
        markState(canvas, irect, state);
        break;

    case WebThemeEngine::PartTextField:
        paint.setColor(extraParams->textField.backgroundColor);
        paint.setStyle(SkPaint::kFill_Style);
        canvas->drawIRect(irect, paint);

        paint.setColor(edgeColor);
        paint.setStyle(SkPaint::kStroke_Style);
        canvas->drawIRect(irect, paint);

        markState(canvas, irect, state);
        break;

    case WebThemeEngine::PartMenuList:
        if (extraParams->menuList.fillContentArea) {
            box(canvas, irect, extraParams->menuList.backgroundColor);
        } else {
            SkPaint paint;
            paint.setColor(edgeColor);
            paint.setStyle(SkPaint::kStroke_Style);
            canvas->drawIRect(irect, paint);
        }

        // clip the drop-down arrow to be inside the select box
        if (extraParams->menuList.arrowX - 4 > irect.fLeft)
            irect.fLeft = extraParams->menuList.arrowX - 4;
        if (extraParams->menuList.arrowX + 12 < irect.fRight)
            irect.fRight = extraParams->menuList.arrowX + 12;

        irect.fTop = extraParams->menuList.arrowY - (extraParams->menuList.arrowHeight) / 2;
        irect.fBottom = extraParams->menuList.arrowY + (extraParams->menuList.arrowHeight - 1) / 2;
        halfWidth = irect.width() / 2;
        quarterWidth = irect.width() / 4;

        if (state == WebThemeEngine::StateFocused) // FIXME: draw differenty?
            state = WebThemeEngine::StateNormal;
        box(canvas, irect, bgColors(state));
        triangle(canvas,
            irect.fLeft  + quarterWidth, irect.fTop,
            irect.fRight - quarterWidth, irect.fTop,
            irect.fLeft  + halfWidth,    irect.fBottom,
            edgeColor);

        break;

    case WebThemeEngine::PartSliderTrack: {
        SkIRect lirect =  irect;

        // Draw a narrow rect for the track plus box hatches on the ends.
        if (state == WebThemeEngine::StateFocused) // FIXME: draw differently?
            state = WebThemeEngine::StateNormal;
        if (extraParams->slider.vertical) {
            lirect.inset(halfWidth - sliderIndent, noOffset);
            box(canvas, lirect, bgColors(state));
            line(canvas, left, top, right, top, edgeColor);
            line(canvas, left, bottom, right, bottom, edgeColor);
        } else {
            lirect.inset(noOffset, halfHeight - sliderIndent);
            box(canvas, lirect, bgColors(state));
            line(canvas, left, top, left, bottom, edgeColor);
            line(canvas, right, top, right, bottom, edgeColor);
        }
        break;
    }

    case WebThemeEngine::PartSliderThumb:
        if (state == WebThemeEngine::StateFocused) // FIXME: draw differently?
            state = WebThemeEngine::StateNormal;
        oval(canvas, irect, bgColors(state));
        break;

    case WebThemeEngine::PartInnerSpinButton: {
        // stack half-height up and down arrows on top of each other
        SkIRect lirect;
        int halfHeight = rect.height / 2;
        if (extraParams->innerSpin.readOnly)
            state = blink::WebThemeEngine::StateDisabled;

        lirect.set(rect.x, rect.y, rect.x + rect.width - 1, rect.y + halfHeight - 1);
        box(canvas, lirect, bgColors(state));
        bottom = lirect.fBottom;
        quarterHeight = lirect.height() / 4;
        triangle(canvas,
            left  + quarterWidth, bottom - quarterHeight,
            right - quarterWidth, bottom - quarterHeight,
            left  + halfWidth,    top    + quarterHeight,
            edgeColor);

        lirect.set(rect.x, rect.y + halfHeight, rect.x + rect.width - 1,
            rect.y + 2 * halfHeight - 1);
        top = lirect.fTop;
        bottom = lirect.fBottom;
        quarterHeight = lirect.height() / 4;
        box(canvas, lirect, bgColors(state));
        triangle(canvas,
            left  + quarterWidth, top    + quarterHeight,
            right - quarterWidth, top    + quarterHeight,
            left  + halfWidth,    bottom - quarterHeight,
            edgeColor);
        markState(canvas, irect, state);
        break;
    }
    case WebThemeEngine::PartProgressBar: {
        paint.setColor(bgColors(state));
        paint.setStyle(SkPaint::kFill_Style);
        canvas->drawIRect(irect, paint);

        // Emulate clipping
        SkIRect tofill = irect;
        if (extraParams->progressBar.determinate) {
            tofill.set(extraParams->progressBar.valueRectX,
                extraParams->progressBar.valueRectY,
                extraParams->progressBar.valueRectX +
                extraParams->progressBar.valueRectWidth - 1,
                extraParams->progressBar.valueRectY +
                extraParams->progressBar.valueRectHeight);
        }

        tofill.intersect(irect, tofill);
        paint.setColor(edgeColor);
        paint.setStyle(SkPaint::kFill_Style);
        canvas->drawIRect(tofill, paint);

        markState(canvas, irect, state);
        break;
    }
    default:
        // FIXME: Should we do something here to indicate that we got an invalid part?
        // Unfortunately, we can't assert because we don't have access to WTF or base.
        break;
    }
}

}  // namespace content
