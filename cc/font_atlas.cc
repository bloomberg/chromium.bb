// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/font_atlas.h"

#include <vector>

#include "base/string_split.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

FontAtlas::FontAtlas(SkBitmap bitmap, gfx::Rect asciiToRectTable[128], int fontHeight)
    : m_atlas(bitmap)
    , m_fontHeight(fontHeight)
{
    for (size_t i = 0; i < 128; ++i)
        m_asciiToRectTable[i] = asciiToRectTable[i];
}

FontAtlas::~FontAtlas()
{
}

void FontAtlas::drawText(SkCanvas* canvas, const SkPaint& paint, const std::string& text, const gfx::Point& destPosition, const gfx::Size& clip) const
{
    std::vector<std::string> lines;
    base::SplitString(text, '\n', &lines);

    gfx::Point position = destPosition;
    for (size_t i = 0; i < lines.size(); ++i) {
        drawOneLineOfTextInternal(canvas, paint, lines[i], position);
        position.set_y(position.y() + m_fontHeight);
        if (position.y() > clip.height())
            return;
    }
}

void FontAtlas::drawOneLineOfTextInternal(SkCanvas* canvas, const SkPaint& paint, const std::string& textLine, const gfx::Point& destPosition) const
{
    gfx::Point position = destPosition;
    for (unsigned i = 0; i < textLine.length(); ++i) {
        // If the ASCII code is out of bounds, then index 0 is used, which is just a plain rectangle glyph.
        unsigned asciiIndex = textLine[i];
        if (asciiIndex >= 128)
          asciiIndex = 0;
        gfx::Rect glyphBounds = m_asciiToRectTable[asciiIndex];
        SkIRect source = SkIRect::MakeXYWH(glyphBounds.x(), glyphBounds.y(), glyphBounds.width(), glyphBounds.height());
        canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(position.x(), position.y(), glyphBounds.width(), glyphBounds.height()), &paint);
        position.set_x(position.x() + glyphBounds.width());
    }
}

gfx::Size FontAtlas::textSize(const std::string& text)
{
    int maxWidth = 0;
    std::vector<std::string> lines;
    base::SplitString(text, '\n', &lines);

    for (size_t i = 0; i < lines.size(); ++i) {
        int lineWidth = 0;
        for (size_t j = 0; j < lines[i].size(); ++j) {
            unsigned asciiIndex = lines[i][j];
            if (asciiIndex >= 128)
              asciiIndex = 0;
            lineWidth += m_asciiToRectTable[asciiIndex].width();
        }
        if (lineWidth > maxWidth)
            maxWidth = lineWidth;
    }

    return gfx::Size(maxWidth, m_fontHeight * lines.size());
}

void FontAtlas::drawDebugAtlas(SkCanvas* canvas, const gfx::Point& destPosition) const
{
    SkIRect source = SkIRect::MakeWH(m_atlas.width(), m_atlas.height());
    canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(destPosition.x(), destPosition.y(), m_atlas.width(), m_atlas.height()));
}

}  // namespace cc
