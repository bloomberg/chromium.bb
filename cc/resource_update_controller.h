// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_UPDATE_CONTROLLER_H_
#define CC_RESOURCE_UPDATE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/cc_export.h"
#include "cc/resource_update_queue.h"

namespace cc {

class ResourceProvider;
class Thread;

class ResourceUpdateControllerClient {
public:
    virtual void readyToFinalizeTextureUpdates() = 0;

protected:
    virtual ~ResourceUpdateControllerClient() { }
};

class CC_EXPORT ResourceUpdateController {
public:
    static scoped_ptr<ResourceUpdateController> create(ResourceUpdateControllerClient* client, Thread* thread, scoped_ptr<ResourceUpdateQueue> queue, ResourceProvider* resourceProvider)
    {
        return make_scoped_ptr(new ResourceUpdateController(client, thread, queue.Pass(), resourceProvider));
    }
    static size_t maxPartialTextureUpdates();

    virtual ~ResourceUpdateController();

    // Discard uploads to textures that were evicted on the impl thread.
    void discardUploadsToEvictedResources();

    void performMoreUpdates(base::TimeTicks timeLimit);
    void finalize();


    // Virtual for testing.
    virtual base::TimeTicks now() const;
    virtual base::TimeDelta updateMoreTexturesTime() const;
    virtual size_t updateMoreTexturesSize() const;

protected:
    ResourceUpdateController(ResourceUpdateControllerClient*, Thread*, scoped_ptr<ResourceUpdateQueue>, ResourceProvider*);

private:
    static size_t maxFullUpdatesPerTick(ResourceProvider*);

    size_t maxBlockingUpdates() const;
    base::TimeDelta pendingUpdateTime() const;

    void updateTexture(ResourceUpdate);

    // This returns true when there were textures left to update.
    bool updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();
    void onTimerFired();

    ResourceUpdateControllerClient* m_client;
    scoped_ptr<ResourceUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    ResourceProvider* m_resourceProvider;
    base::TimeTicks m_timeLimit;
    size_t m_textureUpdatesPerTick;
    bool m_firstUpdateAttempt;
    Thread* m_thread;
    base::WeakPtrFactory<ResourceUpdateController> m_weakFactory;
    bool m_taskPosted;

    DISALLOW_COPY_AND_ASSIGN(ResourceUpdateController);
};

}  // namespace cc

#endif  // CC_RESOURCE_UPDATE_CONTROLLER_H_
