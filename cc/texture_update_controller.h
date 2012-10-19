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

class ResourceProvider;

class TextureUpdateControllerClient {
public:
    virtual void readyToFinalizeTextureUpdates() = 0;

protected:
    virtual ~TextureUpdateControllerClient() { }
};

class TextureUpdateController : public TimerClient {
public:
    static scoped_ptr<TextureUpdateController> create(TextureUpdateControllerClient* client, Thread* thread, scoped_ptr<TextureUpdateQueue> queue, ResourceProvider* resourceProvider)
    {
        return make_scoped_ptr(new TextureUpdateController(client, thread, queue.Pass(), resourceProvider));
    }
    static size_t maxPartialTextureUpdates();

    virtual ~TextureUpdateController();

    // Discard uploads to textures that were evicted on the impl thread.
    void discardUploadsToEvictedResources();

    void performMoreUpdates(base::TimeTicks timeLimit);
    void finalize();

    // TimerClient implementation.
    virtual void onTimerFired() OVERRIDE;

    // Virtual for testing.
    virtual base::TimeTicks now() const;
    virtual base::TimeDelta updateMoreTexturesTime() const;
    virtual size_t updateMoreTexturesSize() const;

protected:
    TextureUpdateController(TextureUpdateControllerClient*, Thread*, scoped_ptr<TextureUpdateQueue>, ResourceProvider*);

    static size_t maxFullUpdatesPerTick(ResourceProvider*);

    size_t maxBlockingUpdates() const;

    void updateTexture(ResourceUpdate);

    // This returns true when there were textures left to update.
    bool updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();

    TextureUpdateControllerClient* m_client;
    scoped_ptr<Timer> m_timer;
    scoped_ptr<TextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    ResourceProvider* m_resourceProvider;
    base::TimeTicks m_timeLimit;
    size_t m_textureUpdatesPerTick;
    bool m_firstUpdateAttempt;

private:
    DISALLOW_COPY_AND_ASSIGN(TextureUpdateController);
};

}  // namespace cc

#endif // CCTextureUpdateController_h
