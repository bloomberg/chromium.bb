// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateController_h
#define CCTextureUpdateController_h

#include "base/time.h"
#include "CCTextureUpdateQueue.h"
#include "CCTimer.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace cc {

class TextureUploader;

class CCTextureUpdateControllerClient {
public:
    virtual void readyToFinalizeTextureUpdates() = 0;

protected:
    virtual ~CCTextureUpdateControllerClient() { }
};

class CCTextureUpdateController : public CCTimerClient {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateController);
public:
    static PassOwnPtr<CCTextureUpdateController> create(CCTextureUpdateControllerClient* client, CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
    {
        return adoptPtr(new CCTextureUpdateController(client, thread, queue, resourceProvider, uploader));
    }
    static size_t maxPartialTextureUpdates();

    virtual ~CCTextureUpdateController();

    // Discard uploads to textures that were evicted on the impl thread.
    void discardUploadsToEvictedResources();

    void performMoreUpdates(base::TimeTicks timeLimit);
    void finalize();

    // CCTimerClient implementation.
    virtual void onTimerFired() OVERRIDE;

    // Virtual for testing.
    virtual base::TimeTicks now() const;
    virtual base::TimeDelta updateMoreTexturesTime() const;
    virtual size_t updateMoreTexturesSize() const;

protected:
    CCTextureUpdateController(CCTextureUpdateControllerClient*, CCThread*, PassOwnPtr<CCTextureUpdateQueue>, CCResourceProvider*, TextureUploader*);

    static size_t maxFullUpdatesPerTick(TextureUploader*);

    // This returns true when there were textures left to update.
    bool updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();

    CCTextureUpdateControllerClient* m_client;
    OwnPtr<CCTimer> m_timer;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    CCResourceProvider* m_resourceProvider;
    TextureUploader* m_uploader;
    base::TimeTicks m_timeLimit;
    size_t m_textureUpdatesPerTick;
    bool m_firstUpdateAttempt;
};

}

#endif // CCTextureUpdateController_h
