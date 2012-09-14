// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef HeadsUpDisplayLayerChromium_h
#define HeadsUpDisplayLayerChromium_h

#include "CCFontAtlas.h"
#include "IntSize.h"
#include "LayerChromium.h"

namespace cc {

class HeadsUpDisplayLayerChromium : public LayerChromium {
public:
    static PassRefPtr<HeadsUpDisplayLayerChromium> create();
    virtual ~HeadsUpDisplayLayerChromium();

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE { return true; }

    void setFontAtlas(PassOwnPtr<CCFontAtlas>);

    virtual PassOwnPtr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

protected:
    HeadsUpDisplayLayerChromium();

private:
    OwnPtr<CCFontAtlas> m_fontAtlas;
};

}
#endif
