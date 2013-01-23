// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/font_atlas.h"

#include <vector>

#include "base/string_split.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"

namespace {

// The FontAtlas uses a bitmap with pre-rendered RED glyps to display text.
// Therefore using SkPaint::setColor() has no effect on the font's color when text is drawn.
// This helper class uses color filtering as a workaround to change the font's color.
class ScopedFontColorFilter {
public:
    ScopedFontColorFilter()
        : m_paint(0)
    {
    }

    void init(SkPaint* paint, const SkColor& color)
    {
        if (color == SK_ColorRED)
            return;

        SkColorMatrix matrix;
        memset(matrix.fMat, 0, sizeof(matrix.fMat));

        // Set the matrix to transform the pre-rendered RED font into the provided color.
        matrix.fMat[5 * 0] = SkColorGetR(color) / 255.0;
        matrix.fMat[5 * 1] = SkColorGetG(color) / 255.0;
        matrix.fMat[5 * 2] = SkColorGetB(color) / 255.0;
        matrix.fMat[5 * 3 + 3] = SkColorGetA(color) / 255.0;

        // SkPaint can only use one color filter, so the old one needs to be saved.
        SkColorFilter* oldFilter = paint->getColorFilter();
        if (oldFilter) {
            // Increase the reference count, so the old filter is not released when it's replaced.
            oldFilter->ref();

            // Save the old filter for later restoring.
            // Reference count will get decreased on destruction.
            m_oldFilter = skia::AdoptRef(oldFilter);

            // If the old filter had a color matrix, the new filter will use both.
            SkColorMatrix oldMatrix;
            if (oldFilter->asColorMatrix(oldMatrix.fMat))
                matrix.setConcat(oldMatrix, matrix);
        }

        // Set the new color filter and replace the old one.
        skia::RefPtr<SkColorMatrixFilter> filter = skia::AdoptRef(new SkColorMatrixFilter(matrix));
        paint->setColorFilter(filter.get());

        m_paint = paint;
    }

    ~ScopedFontColorFilter()
    {
        // Remove the color filter and restore the old one.
        if (m_paint)
            m_paint->setColorFilter(m_oldFilter.get());
    }

private:
    SkPaint* m_paint;
    skia::RefPtr<SkColorFilter> m_oldFilter;
};

}  // namespace

namespace cc {

FontAtlas::FontAtlas(SkBitmap bitmap, gfx::Rect asciiToRectTable[128], int fontHeight)
    : m_atlas(bitmap)
    , m_fontHeight(fontHeight)
    , m_color(SK_ColorRED)
{
    for (size_t i = 0; i < 128; ++i)
        m_asciiToRectTable[i] = asciiToRectTable[i];
}

FontAtlas::~FontAtlas()
{
}

void FontAtlas::drawText(SkCanvas* canvas, SkPaint* paint, const std::string& text, const gfx::Point& destPosition, const gfx::Size& clip) const
{
    ScopedFontColorFilter filter;
    filter.init(paint, m_color);

    std::vector<std::string> lines;
    base::SplitString(text, '\n', &lines);

    gfx::Point position = destPosition;
    for (size_t i = 0; i < lines.size(); ++i) {
        drawOneLineOfTextInternal(canvas, *paint, lines[i], position);
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
