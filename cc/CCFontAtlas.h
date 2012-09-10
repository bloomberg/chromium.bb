// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCFontAtlas_h
#define CCFontAtlas_h

#if USE(ACCELERATED_COMPOSITING)

#include "IntRect.h"
#include "SkBitmap.h"
#include <string>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

class SkCanvas;

namespace WebCore {

class Color;
class FontDescription;
class GraphicsContext;
class IntPoint;
class IntSize;

// This class provides basic ability to draw text onto the heads-up display.
class CCFontAtlas {
    WTF_MAKE_NONCOPYABLE(CCFontAtlas);
public:
    static PassOwnPtr<CCFontAtlas> create(SkBitmap bitmap, IntRect asciiToRectTable[128], int fontHeight)
    {
        return adoptPtr(new CCFontAtlas(bitmap, asciiToRectTable, fontHeight));
    }
    ~CCFontAtlas();

    // Draws multiple lines of text where each line of text is separated by '\n'.
    // - Correct glyphs will be drawn for ASCII codes in the range 32-127; any characters
    //   outside that range will be displayed as a default rectangle glyph.
    // - IntSize clip is used to avoid wasting time drawing things that are outside the
    //   target canvas bounds.
    // - Should only be called only on the impl thread.
    void drawText(SkCanvas*, const SkPaint&, const std::string& text, const IntPoint& destPosition, const IntSize& clip) const;

    // Draws the entire atlas at the specified position, just for debugging purposes.
    void drawDebugAtlas(SkCanvas*, const IntPoint& destPosition) const;

private:
    CCFontAtlas(SkBitmap, IntRect asciiToRectTable[128], int fontHeight);

    void drawOneLineOfTextInternal(SkCanvas*, const SkPaint&, const std::string&, const IntPoint& destPosition) const;

    // The actual texture atlas containing all the pre-rendered glyphs.
    SkBitmap m_atlas;

    // The look-up tables mapping ascii characters to their IntRect locations on the atlas.
    IntRect m_asciiToRectTable[128];

    int m_fontHeight;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
