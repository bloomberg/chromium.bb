// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTiledLayerTestCommon_h
#define CCTiledLayerTestCommon_h

#include "CCGraphicsContext.h"
#include "CCPrioritizedTexture.h"
#include "CCResourceProvider.h"
#include "CCTextureUpdateQueue.h"
#include "CCTiledLayerImpl.h"
#include "IntRect.h"
#include "IntSize.h"
#include "LayerTextureUpdater.h"
#include "Region.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include "TiledLayerChromium.h"

namespace WebKitTests {

class FakeTiledLayerChromium;

class FakeLayerTextureUpdater : public cc::LayerTextureUpdater {
public:
    class Texture : public cc::LayerTextureUpdater::Texture {
    public:
        Texture(FakeLayerTextureUpdater*, scoped_ptr<cc::CCPrioritizedTexture>);
        virtual ~Texture();

        virtual void update(cc::CCTextureUpdateQueue&, const cc::IntRect&, const cc::IntSize&, bool, cc::CCRenderingStats&) OVERRIDE;

    private:
        FakeLayerTextureUpdater* m_layer;
    };

    FakeLayerTextureUpdater();
    virtual ~FakeLayerTextureUpdater();

    virtual PassOwnPtr<cc::LayerTextureUpdater::Texture> createTexture(cc::CCPrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum) OVERRIDE;

    virtual void prepareToUpdate(const cc::IntRect& contentRect, const cc::IntSize&, float, float, cc::IntRect& resultingOpaqueRect, cc::CCRenderingStats&) OVERRIDE;
    // Sets the rect to invalidate during the next call to prepareToUpdate(). After the next
    // call to prepareToUpdate() the rect is reset.
    void setRectToInvalidate(const cc::IntRect&, FakeTiledLayerChromium*);
    // Last rect passed to prepareToUpdate().
    const cc::IntRect& lastUpdateRect()  const { return m_lastUpdateRect; }

    // Number of times prepareToUpdate has been invoked.
    int prepareCount() const { return m_prepareCount; }
    void clearPrepareCount() { m_prepareCount = 0; }

    // Number of times update() has been invoked on a texture.
    int updateCount() const { return m_updateCount; }
    void clearUpdateCount() { m_updateCount = 0; }
    void update() { m_updateCount++; }

    void setOpaquePaintRect(const cc::IntRect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

private:
    int m_prepareCount;
    int m_updateCount;
    cc::IntRect m_rectToInvalidate;
    cc::IntRect m_lastUpdateRect;
    cc::IntRect m_opaquePaintRect;
    scoped_refptr<FakeTiledLayerChromium> m_layer;
};

class FakeCCTiledLayerImpl : public cc::CCTiledLayerImpl {
public:
    explicit FakeCCTiledLayerImpl(int id);
    virtual ~FakeCCTiledLayerImpl();

    using cc::CCTiledLayerImpl::hasTileAt;
    using cc::CCTiledLayerImpl::hasResourceIdForTileAt;
};

class FakeTiledLayerChromium : public cc::TiledLayerChromium {
public:
    explicit FakeTiledLayerChromium(cc::CCPrioritizedTextureManager*);

    static cc::IntSize tileSize() { return cc::IntSize(100, 100); }

    using cc::TiledLayerChromium::invalidateContentRect;
    using cc::TiledLayerChromium::needsIdlePaint;
    using cc::TiledLayerChromium::skipsDraw;
    using cc::TiledLayerChromium::numPaintedTiles;
    using cc::TiledLayerChromium::idlePaintRect;

    virtual void setNeedsDisplayRect(const cc::FloatRect&) OVERRIDE;
    const cc::FloatRect& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

    virtual void setTexturePriorities(const cc::CCPriorityCalculator&) OVERRIDE;

    virtual cc::CCPrioritizedTextureManager* textureManager() const OVERRIDE;
    FakeLayerTextureUpdater* fakeLayerTextureUpdater() { return m_fakeTextureUpdater.get(); }
    cc::FloatRect updateRect() { return m_updateRect; }

protected:
    virtual cc::LayerTextureUpdater* textureUpdater() const OVERRIDE;
    virtual void createTextureUpdaterIfNeeded() OVERRIDE { }
    virtual ~FakeTiledLayerChromium();

private:
    RefPtr<FakeLayerTextureUpdater> m_fakeTextureUpdater;
    cc::CCPrioritizedTextureManager* m_textureManager;
    cc::FloatRect m_lastNeedsDisplayRect;
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayerChromium {
public:
    explicit FakeTiledLayerWithScaledBounds(cc::CCPrioritizedTextureManager*);

    void setContentBounds(const cc::IntSize& contentBounds) { m_forcedContentBounds = contentBounds; }
    virtual cc::IntSize contentBounds() const OVERRIDE;

protected:
    virtual ~FakeTiledLayerWithScaledBounds();
    cc::IntSize m_forcedContentBounds;
};


class FakeTextureUploader : public cc::TextureUploader {
public:
    virtual size_t numBlockingUploads() OVERRIDE;
    virtual void markPendingUploadsAsNonBlocking() OVERRIDE;
    virtual double estimatedTexturesPerSecond() OVERRIDE;
    virtual void uploadTexture(cc::CCResourceProvider*, Parameters) OVERRIDE;
};

}
#endif // CCTiledLayerTestCommon_h
