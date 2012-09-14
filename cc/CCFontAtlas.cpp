// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "CCFontAtlas.h"

#include "base/string_split.h"
#include "CCProxy.h"
#include "SkCanvas.h"
#include <vector>

namespace cc {

using namespace std;

CCFontAtlas::CCFontAtlas(SkBitmap bitmap, IntRect asciiToRectTable[128], int fontHeight)
    : m_atlas(bitmap)
    , m_fontHeight(fontHeight)
{
    for (size_t i = 0; i < 128; ++i)
        m_asciiToRectTable[i] = asciiToRectTable[i];
}

CCFontAtlas::~CCFontAtlas()
{
}

void CCFontAtlas::drawText(SkCanvas* canvas, const SkPaint& paint, const std::string& text, const IntPoint& destPosition, const IntSize& clip) const
{
    ASSERT(CCProxy::isImplThread());

    std::vector<std::string> lines;
    base::SplitString(text, '\n', &lines);

    IntPoint position = destPosition;
    for (size_t i = 0; i < lines.size(); ++i) {
        drawOneLineOfTextInternal(canvas, paint, lines[i], position);
        position.setY(position.y() + m_fontHeight);
        if (position.y() > clip.height())
            return;
    }
}

void CCFontAtlas::drawOneLineOfTextInternal(SkCanvas* canvas, const SkPaint& paint, const std::string& textLine, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());

    IntPoint position = destPosition;
    for (unsigned i = 0; i < textLine.length(); ++i) {
        // If the ASCII code is out of bounds, then index 0 is used, which is just a plain rectangle glyph.
        int asciiIndex = (textLine[i] < 128) ? textLine[i] : 0;
        IntRect glyphBounds = m_asciiToRectTable[asciiIndex];
        SkIRect source = SkIRect::MakeXYWH(glyphBounds.x(), glyphBounds.y(), glyphBounds.width(), glyphBounds.height());
        canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(position.x(), position.y(), glyphBounds.width(), glyphBounds.height()), &paint);
        position.setX(position.x() + glyphBounds.width());
    }
}

void CCFontAtlas::drawDebugAtlas(SkCanvas* canvas, const IntPoint& destPosition) const
{
    ASSERT(CCProxy::isImplThread());

    SkIRect source = SkIRect::MakeWH(m_atlas.width(), m_atlas.height());
    canvas->drawBitmapRect(m_atlas, &source, SkRect::MakeXYWH(destPosition.x(), destPosition.y(), m_atlas.width(), m_atlas.height()));
}

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
