// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateController_h
#define CCTextureUpdateController_h

#include "CCTimer.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/texture_update_queue.h"

namespace cc {

class CCResourceProvider;

class CCTextureUpdateControllerClient {
public:
    virtual void readyToFinalizeTextureUpdates() = 0;

protected:
    virtual ~CCTextureUpdateControllerClient() { }
};

class CCTextureUpdateController : public CCTimerClient {
public:
    static scoped_ptr<CCTextureUpdateController> create(CCTextureUpdateControllerClient* client, CCThread* thread, scoped_ptr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider)
    {
        return make_scoped_ptr(new CCTextureUpdateController(client, thread, queue.Pass(), resourceProvider));
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
    CCTextureUpdateController(CCTextureUpdateControllerClient*, CCThread*, scoped_ptr<CCTextureUpdateQueue>, CCResourceProvider*);

    static size_t maxFullUpdatesPerTick(CCResourceProvider*);

    size_t maxBlockingUpdates() const;

    void updateTexture(ResourceUpdate);

    // This returns true when there were textures left to update.
    bool updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();

    CCTextureUpdateControllerClient* m_client;
    scoped_ptr<CCTimer> m_timer;
    scoped_ptr<CCTextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    CCResourceProvider* m_resourceProvider;
    base::TimeTicks m_timeLimit;
    size_t m_textureUpdatesPerTick;
    bool m_firstUpdateAttempt;

private:
    DISALLOW_COPY_AND_ASSIGN(CCTextureUpdateController);
};

}  // namespace cc

#endif // CCTextureUpdateController_h
