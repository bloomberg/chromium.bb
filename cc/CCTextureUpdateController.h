// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateController_h
#define CCTextureUpdateController_h

#include "CCTextureUpdateQueue.h"
#include "CCTimer.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace cc {

class TextureCopier;
class TextureUploader;

class CCTextureUpdateControllerClient {
public:
    virtual void updateTexturesCompleted() = 0;

protected:
    virtual ~CCTextureUpdateControllerClient() { }
};

class CCTextureUpdateController : public CCTimerClient {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateController);
public:
    static PassOwnPtr<CCTextureUpdateController> create(CCTextureUpdateControllerClient* client, CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader)
    {
        return adoptPtr(new CCTextureUpdateController(client, thread, queue, resourceProvider, copier, uploader));
    }
    static size_t maxPartialTextureUpdates();
    static void updateTextures(CCResourceProvider*, TextureCopier*, TextureUploader*, CCTextureUpdateQueue*, size_t count);

    virtual ~CCTextureUpdateController();

    void updateMoreTextures(double monotonicTimeLimit);

    // CCTimerClient implementation.
    virtual void onTimerFired() OVERRIDE;

    // Virtual for testing.
    virtual double monotonicTimeNow() const;
    virtual double updateMoreTexturesTime() const;
    virtual size_t updateMoreTexturesSize() const;

protected:
    CCTextureUpdateController(CCTextureUpdateControllerClient*, CCThread*, PassOwnPtr<CCTextureUpdateQueue>, CCResourceProvider*, TextureCopier*, TextureUploader*);

    // This returns true when there were textures left to update.
    bool updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();

    CCTextureUpdateControllerClient* m_client;
    OwnPtr<CCTimer> m_timer;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    CCResourceProvider* m_resourceProvider;
    TextureCopier* m_copier;
    TextureUploader* m_uploader;
    double m_monotonicTimeLimit;
    bool m_firstUpdateAttempt;
};

}

#endif // CCTextureUpdateController_h
