// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureUpdateController.h"

#include "CCSchedulerTestCommon.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCTiledLayerTestCommon.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "WebCompositorInitializer.h"
#include <gtest/gtest.h>
#include <public/WebThread.h>
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;
using testing::Test;


namespace {

const int kFlushPeriodFull = 4;
const int kFlushPeriodPartial = kFlushPeriodFull;

class CCTextureUpdateControllerTest;

class WebGraphicsContext3DForUploadTest : public FakeWebGraphicsContext3D {
public:
    WebGraphicsContext3DForUploadTest(CCTextureUpdateControllerTest *test)
        : m_test(test)
        , m_supportShallowFlush(true)
    { }

    virtual void flush(void);
    virtual void shallowFlushCHROMIUM(void);
    virtual GrGLInterface* onCreateGrGLInterface() { return 0; }

    virtual WebString getString(WGC3Denum name)
    {
        if (m_supportShallowFlush)
            return WebString("GL_CHROMIUM_shallow_flush");
        return WebString("");
    }

private:
    CCTextureUpdateControllerTest* m_test;
    bool m_supportShallowFlush;
};


class TextureUploaderForUploadTest : public FakeTextureUploader {
public:
    TextureUploaderForUploadTest(CCTextureUpdateControllerTest *test) : m_test(test) { }

    virtual void beginUploads() OVERRIDE;
    virtual void endUploads() OVERRIDE;
    virtual void uploadTexture(cc::CCResourceProvider*, Parameters) OVERRIDE;

private:
    CCTextureUpdateControllerTest* m_test;
};

class TextureForUploadTest : public LayerTextureUpdater::Texture {
public:
    TextureForUploadTest() 
        : LayerTextureUpdater::Texture(nullptr)
        , m_evicted(false)
    {
    }
    virtual void updateRect(CCResourceProvider*, const IntRect& sourceRect, const IntSize& destOffset) { }
    virtual bool backingResourceWasEvicted() const { return m_evicted; }
    void evictBackingResource() { m_evicted = true; }
private:
    bool m_evicted;
};


class CCTextureUpdateControllerTest : public Test {
public:
    CCTextureUpdateControllerTest()
        : m_queue(adoptPtr(new CCTextureUpdateQueue))
        , m_uploader(this)
        , m_compositorInitializer(m_thread.get())
        , m_fullUploadCountExpected(0)
        , m_partialCountExpected(0)
        , m_totalUploadCountExpected(0)
        , m_maxUploadCountPerUpdate(0)
        , m_numBeginUploads(0)
        , m_numEndUploads(0)
        , m_numConsecutiveFlushes(0)
        , m_numDanglingUploads(0)
        , m_numTotalUploads(0)
        , m_numTotalFlushes(0)
        , m_numPreviousUploads(0)
        , m_numPreviousFlushes(0)
    {
    }

public:
    void onFlush()
    {
        // Check for back-to-back flushes.
        EXPECT_EQ(0, m_numConsecutiveFlushes) << "Back-to-back flushes detected.";

        // Check for premature flushes
        if (m_numPreviousUploads != m_maxUploadCountPerUpdate) {
            if (m_numTotalUploads < m_fullUploadCountExpected)
                EXPECT_GE(m_numDanglingUploads, kFlushPeriodFull) << "Premature flush detected in full uploads.";
            else if (m_numTotalUploads > m_fullUploadCountExpected && m_numTotalUploads < m_totalUploadCountExpected)
                EXPECT_GE(m_numDanglingUploads, kFlushPeriodPartial) << "Premature flush detected in partial uploads.";
        }

        m_numDanglingUploads = 0;
        m_numConsecutiveFlushes++;
        m_numTotalFlushes++;
        m_numPreviousFlushes++;
    }

    void onBeginUploads()
    {
        EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";

        m_numPreviousFlushes = 0;
        m_numPreviousUploads = 0;
        m_numBeginUploads++;
    }

    void onUpload()
    {
        // Check for too many consecutive uploads
        if (m_numTotalUploads < m_fullUploadCountExpected)
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodFull) << "Too many consecutive full uploads detected.";
        else
            EXPECT_LT(m_numDanglingUploads, kFlushPeriodPartial) << "Too many consecutive partial uploads detected.";

        m_numConsecutiveFlushes = 0;
        m_numDanglingUploads++;
        m_numTotalUploads++;
        m_numPreviousUploads++;
    }

    void onEndUploads()
    {
        // Note: The m_numTotalUploads != m_fullUploadCountExpected comparison
        // allows for the quota not to be hit in the case where we are trasitioning
        // from full uploads to partial uploads.
        if (m_numTotalUploads != m_totalUploadCountExpected && m_numTotalUploads != m_fullUploadCountExpected) {
            EXPECT_EQ(m_maxUploadCountPerUpdate, m_numPreviousUploads)
                << "endUpload() was called when there are textures to upload, but the upload quota hasn't been filled.";
        }

        m_numEndUploads++;
    }

protected:
    virtual void SetUp()
    {
        m_context = FakeWebCompositorOutputSurface::create(adoptPtr(new WebGraphicsContext3DForUploadTest(this)));
        DebugScopedSetImplThread implThread;
        m_resourceProvider = CCResourceProvider::create(m_context.get(), UnthrottledUploader);
    }


    void appendFullUploadsOfIndexedTextureToUpdateQueue(int count, int textureIndex)
    {
        m_fullUploadCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 300, 150);
        const TextureUploader::Parameters upload = { &m_textures[textureIndex], rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendFullUpload(upload);
    }

    void appendFullUploadsToUpdateQueue(int count)
    {
        appendFullUploadsOfIndexedTextureToUpdateQueue(count, 0);
    }

    void appendPartialUploadsOfIndexedTextureToUpdateQueue(int count, int textureIndex)
    {
        m_partialCountExpected += count;
        m_totalUploadCountExpected += count;

        const IntRect rect(0, 0, 100, 100);
        const TextureUploader::Parameters upload = { &m_textures[textureIndex], rect, IntSize() };
        for (int i = 0; i < count; i++)
            m_queue->appendPartialUpload(upload);
    }

    void appendPartialUploadsToUpdateQueue(int count)
    {
        appendPartialUploadsOfIndexedTextureToUpdateQueue(count, 0);
    }

    void setMaxUploadCountPerUpdate(int count)
    {
        m_maxUploadCountPerUpdate = count;
    }

    void updateTextures()
    {
        OwnPtr<CCTextureUpdateController> updateController =
            CCTextureUpdateController::create(
                NULL,
                CCProxy::implThread(),
                m_queue.release(),
                m_resourceProvider.get(),
                &m_uploader);
        updateController->finalize();
    }

protected:
    // Classes required to interact and test the CCTextureUpdateController
    OwnPtr<CCGraphicsContext> m_context;
    OwnPtr<CCResourceProvider> m_resourceProvider;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    TextureForUploadTest m_textures[4];
    TextureUploaderForUploadTest m_uploader;
    OwnPtr<WebThread> m_thread;
    WebCompositorInitializer m_compositorInitializer;


    // Properties / expectations of this test
    int m_fullUploadCountExpected;
    int m_partialCountExpected;
    int m_totalUploadCountExpected;
    int m_maxUploadCountPerUpdate;

    // Dynamic properties of this test
    int m_numBeginUploads;
    int m_numEndUploads;
    int m_numConsecutiveFlushes;
    int m_numDanglingUploads;
    int m_numTotalUploads;
    int m_numTotalFlushes;
    int m_numPreviousUploads;
    int m_numPreviousFlushes;
};

void WebGraphicsContext3DForUploadTest::flush(void)
{
    m_test->onFlush();
}

void WebGraphicsContext3DForUploadTest::shallowFlushCHROMIUM(void)
{
    m_test->onFlush();
}

void TextureUploaderForUploadTest::beginUploads()
{
    m_test->onBeginUploads();
}

void TextureUploaderForUploadTest::endUploads()
{
    m_test->onEndUploads();
}

void TextureUploaderForUploadTest::uploadTexture(cc::CCResourceProvider*, Parameters)
{
    m_test->onUpload();
}

// ZERO UPLOADS TESTS
TEST_F(CCTextureUpdateControllerTest, ZeroUploads)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(0, m_numPreviousFlushes);
    EXPECT_EQ(0, m_numPreviousUploads);
}


// ONE UPLOAD TESTS
TEST_F(CCTextureUpdateControllerTest, OneFullUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numPreviousFlushes);
    EXPECT_EQ(1, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, OnePartialUpload)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numPreviousFlushes);
    EXPECT_EQ(1, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, OneFullOnePartialUpload)
{
    appendFullUploadsToUpdateQueue(1);
    appendPartialUploadsToUpdateQueue(1);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(1, m_numPreviousFlushes);
    EXPECT_EQ(2, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}


// This class of tests upload a number of textures that is a multiple of the flush period.
const int fullUploadFlushMultipler = 7;
const int fullCount = fullUploadFlushMultipler * kFlushPeriodFull;

const int partialUploadFlushMultipler = 11;
const int partialCount = partialUploadFlushMultipler * kFlushPeriodPartial;

TEST_F(CCTextureUpdateControllerTest, ManyFullUploads)
{
    appendFullUploadsToUpdateQueue(fullCount);
    appendPartialUploadsToUpdateQueue(0);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(fullUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullCount, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, ManyPartialUploads)
{
    appendFullUploadsToUpdateQueue(0);
    appendPartialUploadsToUpdateQueue(partialCount);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(partialCount, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

TEST_F(CCTextureUpdateControllerTest, ManyFullManyPartialUploads)
{
    appendFullUploadsToUpdateQueue(fullCount);
    appendPartialUploadsToUpdateQueue(partialCount);
    DebugScopedSetImplThread implThread;
    updateTextures();

    EXPECT_EQ(fullUploadFlushMultipler + partialUploadFlushMultipler, m_numPreviousFlushes);
    EXPECT_EQ(fullCount + partialCount, m_numPreviousUploads);
    EXPECT_EQ(0, m_numDanglingUploads) << "Last upload wasn't followed by a flush.";
}

class FakeCCTextureUpdateControllerClient : public cc::CCTextureUpdateControllerClient {
public:
    FakeCCTextureUpdateControllerClient() { reset(); }
    void reset() { m_readyToFinalizeCalled = false; }
    bool readyToFinalizeCalled() const { return m_readyToFinalizeCalled; }

    virtual void readyToFinalizeTextureUpdates() OVERRIDE { m_readyToFinalizeCalled = true; }

protected:
    bool m_readyToFinalizeCalled;
};

class FakeCCTextureUpdateController : public cc::CCTextureUpdateController {
public:
    static PassOwnPtr<FakeCCTextureUpdateController> create(cc::CCTextureUpdateControllerClient* client, cc::CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
    {
        return adoptPtr(new FakeCCTextureUpdateController(client, thread, queue, resourceProvider, uploader));
    }

    void setMonotonicTimeNow(double time) { m_monotonicTimeNow = time; }
    virtual double monotonicTimeNow() const OVERRIDE { return m_monotonicTimeNow; }
    void setUpdateMoreTexturesTime(double time) { m_updateMoreTexturesTime = time; }
    virtual double updateMoreTexturesTime() const OVERRIDE { return m_updateMoreTexturesTime; }
    void setUpdateMoreTexturesSize(size_t size) { m_updateMoreTexturesSize = size; }
    virtual size_t updateMoreTexturesSize() const OVERRIDE { return m_updateMoreTexturesSize; }

protected:
    FakeCCTextureUpdateController(cc::CCTextureUpdateControllerClient* client, cc::CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
        : cc::CCTextureUpdateController(client, thread, queue, resourceProvider, uploader)
        , m_monotonicTimeNow(0)
        , m_updateMoreTexturesTime(0)
        , m_updateMoreTexturesSize(0) { }

    double m_monotonicTimeNow;
    double m_updateMoreTexturesTime;
    size_t m_updateMoreTexturesSize;
};

static void runPendingTask(FakeCCThread* thread, FakeCCTextureUpdateController* controller)
{
    EXPECT_TRUE(thread->hasPendingTask());
    controller->setMonotonicTimeNow(controller->monotonicTimeNow() + thread->pendingDelayMs() / 1000.0);
    thread->runPendingTask();
}

TEST_F(CCTextureUpdateControllerTest, UpdateMoreTextures)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(3);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.1);
    controller->setUpdateMoreTexturesSize(1);
    // Not enough time for any updates.
    controller->performMoreUpdates(0.09);
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_EQ(0, m_numBeginUploads);
    EXPECT_EQ(0, m_numEndUploads);

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.1);
    controller->setUpdateMoreTexturesSize(1);
    // Only enough time for 1 update.
    controller->performMoreUpdates(0.12);
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_EQ(1, m_numBeginUploads);
    EXPECT_EQ(1, m_numEndUploads);
    EXPECT_EQ(1, m_numTotalUploads);

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.1);
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for 2 updates.
    controller->performMoreUpdates(0.22);
    runPendingTask(&thread, controller.get());
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(3, m_numBeginUploads);
    EXPECT_EQ(3, m_numEndUploads);
    EXPECT_EQ(3, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, NoMoreUpdates)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(2);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.1);
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for 3 updates but only 2 necessary.
    controller->performMoreUpdates(0.31);
    runPendingTask(&thread, controller.get());
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numBeginUploads);
    EXPECT_EQ(2, m_numEndUploads);
    EXPECT_EQ(2, m_numTotalUploads);

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.1);
    controller->setUpdateMoreTexturesSize(1);
    // Enough time for updates but no more updates left.
    controller->performMoreUpdates(0.31);
    // 0-delay task used to call readyToFinalizeTextureUpdates().
    runPendingTask(&thread, controller.get());
    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numBeginUploads);
    EXPECT_EQ(2, m_numEndUploads);
    EXPECT_EQ(2, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, UpdatesCompleteInFiniteTime)
{
    FakeCCTextureUpdateControllerClient client;
    FakeCCThread thread;

    setMaxUploadCountPerUpdate(1);
    appendFullUploadsToUpdateQueue(2);
    appendPartialUploadsToUpdateQueue(0);

    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTextureUpdateController> controller(FakeCCTextureUpdateController::create(&client, &thread, m_queue.release(), m_resourceProvider.get(), &m_uploader));

    controller->setMonotonicTimeNow(0);
    controller->setUpdateMoreTexturesTime(0.5);
    controller->setUpdateMoreTexturesSize(1);

    for (int i = 0; i < 100; i++) {
        if (client.readyToFinalizeCalled())
            break;

        // Not enough time for any updates.
        controller->performMoreUpdates(0.4);

        if (thread.hasPendingTask())
            runPendingTask(&thread, controller.get());
    }

    EXPECT_FALSE(thread.hasPendingTask());
    EXPECT_TRUE(client.readyToFinalizeCalled());
    EXPECT_EQ(2, m_numBeginUploads);
    EXPECT_EQ(2, m_numEndUploads);
    EXPECT_EQ(2, m_numTotalUploads);
}

TEST_F(CCTextureUpdateControllerTest, ClearUploadsToEvictedResources)
{
    appendFullUploadsOfIndexedTextureToUpdateQueue(1, 0);
    appendPartialUploadsOfIndexedTextureToUpdateQueue(1, 1);
    appendFullUploadsOfIndexedTextureToUpdateQueue(1, 2);
    appendPartialUploadsOfIndexedTextureToUpdateQueue(1, 3);
    DebugScopedSetImplThread implThread;

    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(2u, m_queue->fullUploadSize());
    EXPECT_EQ(2u, m_queue->partialUploadSize());

    m_textures[0].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(1u, m_queue->fullUploadSize());
    EXPECT_EQ(2u, m_queue->partialUploadSize());

    m_textures[3].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(1u, m_queue->fullUploadSize());
    EXPECT_EQ(1u, m_queue->partialUploadSize());

    m_textures[2].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(0u, m_queue->fullUploadSize());
    EXPECT_EQ(1u, m_queue->partialUploadSize());

    m_textures[1].evictBackingResource();
    m_queue->clearUploadsToEvictedResources();
    EXPECT_EQ(0u, m_queue->fullUploadSize());
    EXPECT_EQ(0u, m_queue->partialUploadSize());
}

} // namespace
