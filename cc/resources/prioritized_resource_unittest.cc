// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource.h"

#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/single_thread_proxy.h" // For DebugScopedSetImplThread
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class PrioritizedResourceTest : public testing::Test {
public:
    PrioritizedResourceTest()
        : m_proxy(scoped_ptr<Thread>(NULL))
        , m_textureSize(256, 256)
        , m_textureFormat(GL_RGBA)
        , m_outputSurface(createFakeOutputSurface())
    {
        DebugScopedSetImplThread implThread(&m_proxy);
        m_resourceProvider = ResourceProvider::Create(m_outputSurface.get());
    }

    virtual ~PrioritizedResourceTest()
    {
        DebugScopedSetImplThread implThread(&m_proxy);
        m_resourceProvider.reset();
    }

    size_t texturesMemorySize(size_t textureCount)
    {
        return Resource::MemorySizeBytes(m_textureSize, m_textureFormat) * textureCount;
    }

    scoped_ptr<PrioritizedResourceManager> createManager(size_t maxTextures)
    {
        scoped_ptr<PrioritizedResourceManager> manager = PrioritizedResourceManager::create(&m_proxy);
        manager->setMaxMemoryLimitBytes(texturesMemorySize(maxTextures));
        return manager.Pass();
    }

    bool validateTexture(scoped_ptr<PrioritizedResource>& texture, bool requestLate)
    {
        resourceManagerAssertInvariants(texture->resource_manager());
        if (requestLate)
            texture->RequestLate();
        resourceManagerAssertInvariants(texture->resource_manager());
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        bool success = texture->can_acquire_backing_texture();
        if (success)
            texture->AcquireBackingTexture(resourceProvider());
        return success;
    }

    void prioritizeTexturesAndBackings(PrioritizedResourceManager* resourceManager)
    {
        resourceManager->prioritizeTextures();
        resourceManagerUpdateBackingsPriorities(resourceManager);
    }

    void resourceManagerUpdateBackingsPriorities(PrioritizedResourceManager* resourceManager)
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->pushTexturePrioritiesToBackings();
    }

    ResourceProvider* resourceProvider()
    {
       return m_resourceProvider.get();
    }

    void resourceManagerAssertInvariants(PrioritizedResourceManager* resourceManager)
    {
#ifndef NDEBUG
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->assertInvariants();
#endif
    }

    bool textureBackingIsAbovePriorityCutoff(PrioritizedResource* texture)
    {
        return texture->backing()->was_above_priority_cutoff_at_last_priority_update();
    }

    size_t evictedBackingCount(PrioritizedResourceManager* resourceManager)
    {
        return resourceManager->m_evictedBackings.size();
    }

protected:
    FakeProxy m_proxy;
    const gfx::Size m_textureSize;
    const GLenum m_textureFormat;
    scoped_ptr<OutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
};

namespace {

TEST_F(PrioritizedResourceTest, requestTextureExceedingMaxLimit)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);

    // Create textures for double our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures*2];

    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set decreasing priorities
    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i]->set_request_priority(100 + i);

    // Only lower half should be available.
    prioritizeTexturesAndBackings(resourceManager.get());
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[7], false));
    EXPECT_FALSE(validateTexture(textures[8], false));
    EXPECT_FALSE(validateTexture(textures[15], false));

    // Set increasing priorities
    for (size_t i = 0; i < maxTextures*2; ++i)
        textures[i]->set_request_priority(100 - i);

    // Only upper half should be available.
    prioritizeTexturesAndBackings(resourceManager.get());
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[7], false));
    EXPECT_TRUE(validateTexture(textures[8], false));
    EXPECT_TRUE(validateTexture(textures[15], false));

    EXPECT_EQ(texturesMemorySize(maxTextures), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, changeMemoryLimits)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Set max limit to 8 textures
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(8));
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        validateTexture(textures[i], false);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(8), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    // Set max limit to 5 textures
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(5));
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 5);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(5), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    // Set max limit to 4 textures
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(4));
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 4);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }

    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, changePriorityCutoff)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Set the cutoff to drop two textures. Try to requestLate on all textures, and
    // make sure that requestLate doesn't work on a texture with equal priority to
    // the cutoff.
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(8));
    resourceManager->setExternalPriorityCutoff(106);
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], true), i < 6);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }
    EXPECT_EQ(texturesMemorySize(6), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    // Set the cutoff to drop two more textures.
    resourceManager->setExternalPriorityCutoff(104);
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 4);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }
    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryAboveCutoffBytes());

    // Do a one-time eviction for one more texture based on priority cutoff
    PrioritizedResourceManager::BackingList evictedBackings;
    resourceManager->unlinkAndClearEvictedBackings();
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemoryOnImplThread(texturesMemorySize(8), 104, resourceProvider());
        EXPECT_EQ(0, evictedBackingCount(resourceManager.get()));
        resourceManager->reduceMemoryOnImplThread(texturesMemorySize(8), 103, resourceProvider());
        EXPECT_EQ(1, evictedBackingCount(resourceManager.get()));
    }
    resourceManager->unlinkAndClearEvictedBackings();
    EXPECT_EQ(texturesMemorySize(3), resourceManager->memoryUseBytes());

    // Re-allocate the the texture after the one-time drop.
    prioritizeTexturesAndBackings(resourceManager.get());
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_EQ(validateTexture(textures[i], false), i < 4);
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->reduceMemory(resourceProvider());
    }
    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, resourceManagerPartialUpdateTextures)
{
    const size_t maxTextures = 4;
    const size_t numTextures = 4;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    scoped_ptr<PrioritizedResource> textures[numTextures];
    scoped_ptr<PrioritizedResource> moreTextures[numTextures];

    for (size_t i = 0; i < numTextures; ++i) {
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);
        moreTextures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);
    }

    for (size_t i = 0; i < numTextures; ++i)
        textures[i]->set_request_priority(200 + i);
    prioritizeTexturesAndBackings(resourceManager.get());

    // Allocate textures which are currently high priority.
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[1], false));
    EXPECT_TRUE(validateTexture(textures[2], false));
    EXPECT_TRUE(validateTexture(textures[3], false));

    EXPECT_TRUE(textures[0]->have_backing_texture());
    EXPECT_TRUE(textures[1]->have_backing_texture());
    EXPECT_TRUE(textures[2]->have_backing_texture());
    EXPECT_TRUE(textures[3]->have_backing_texture());

    for (size_t i = 0; i < numTextures; ++i)
        moreTextures[i]->set_request_priority(100 + i);
    prioritizeTexturesAndBackings(resourceManager.get());

    // Textures are now below cutoff.
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[1], false));
    EXPECT_FALSE(validateTexture(textures[2], false));
    EXPECT_FALSE(validateTexture(textures[3], false));

    // But they are still valid to use.
    EXPECT_TRUE(textures[0]->have_backing_texture());
    EXPECT_TRUE(textures[1]->have_backing_texture());
    EXPECT_TRUE(textures[2]->have_backing_texture());
    EXPECT_TRUE(textures[3]->have_backing_texture());

    // Higher priority textures are finally needed.
    EXPECT_TRUE(validateTexture(moreTextures[0], false));
    EXPECT_TRUE(validateTexture(moreTextures[1], false));
    EXPECT_TRUE(validateTexture(moreTextures[2], false));
    EXPECT_TRUE(validateTexture(moreTextures[3], false));

    // Lower priority have been fully evicted.
    EXPECT_FALSE(textures[0]->have_backing_texture());
    EXPECT_FALSE(textures[1]->have_backing_texture());
    EXPECT_FALSE(textures[2]->have_backing_texture());
    EXPECT_FALSE(textures[3]->have_backing_texture());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, resourceManagerPrioritiesAreEqual)
{
    const size_t maxTextures = 16;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // All 16 textures have the same priority except 2 higher priority.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100);
    textures[0]->set_request_priority(99);
    textures[1]->set_request_priority(99);

    // Set max limit to 8 textures
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(8));
    prioritizeTexturesAndBackings(resourceManager.get());

    // The two high priority textures should be available, others should not.
    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));
    for (size_t i = 2; i < maxTextures; ++i)
        EXPECT_FALSE(validateTexture(textures[i], false));
    EXPECT_EQ(texturesMemorySize(2), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    // Manually reserving textures should only succeed on the higher priority textures,
    // and on remaining textures up to the memory limit.
    for (size_t i = 0; i < 8; i++)
        EXPECT_TRUE(validateTexture(textures[i], true));
    for (size_t i = 9; i < maxTextures; i++)
        EXPECT_FALSE(validateTexture(textures[i], true));
    EXPECT_EQ(texturesMemorySize(8), resourceManager->memoryAboveCutoffBytes());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, resourceManagerDestroyedFirst)
{
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(1);
    scoped_ptr<PrioritizedResource> texture = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->have_backing_texture());

    texture->set_request_priority(100);
    prioritizeTexturesAndBackings(resourceManager.get());

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture());

    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->clearAllMemory(resourceProvider());
    }
    resourceManager.reset();

    EXPECT_FALSE(texture->can_acquire_backing_texture());
    EXPECT_FALSE(texture->have_backing_texture());
}

TEST_F(PrioritizedResourceTest, textureMovedToNewManager)
{
    scoped_ptr<PrioritizedResourceManager> resourceManagerOne = createManager(1);
    scoped_ptr<PrioritizedResourceManager> resourceManagerTwo = createManager(1);
    scoped_ptr<PrioritizedResource> texture = resourceManagerOne->createTexture(m_textureSize, m_textureFormat);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->have_backing_texture());

    texture->set_request_priority(100);
    prioritizeTexturesAndBackings(resourceManagerOne.get());

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture());

    texture->SetTextureManager(0);

    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManagerOne->clearAllMemory(resourceProvider());
    }
    resourceManagerOne.reset();

    EXPECT_FALSE(texture->can_acquire_backing_texture());
    EXPECT_FALSE(texture->have_backing_texture());

    texture->SetTextureManager(resourceManagerTwo.get());

    prioritizeTexturesAndBackings(resourceManagerTwo.get());

    EXPECT_TRUE(validateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManagerTwo->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, renderSurfacesReduceMemoryAvailableOutsideRootSurface)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> renderSurfacePlaceHolder = resourceManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->SetToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->set_request_priority(PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set decreasing non-visible priorities outside root surface.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Only lower half should be available.
    prioritizeTexturesAndBackings(resourceManager.get());
    EXPECT_TRUE(validateTexture(textures[0], false));
    EXPECT_TRUE(validateTexture(textures[3], false));
    EXPECT_FALSE(validateTexture(textures[4], false));
    EXPECT_FALSE(validateTexture(textures[7], false));

    // Set increasing non-visible priorities outside root surface.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100 - i);

    // Only upper half should be available.
    prioritizeTexturesAndBackings(resourceManager.get());
    EXPECT_FALSE(validateTexture(textures[0], false));
    EXPECT_FALSE(validateTexture(textures[3], false));
    EXPECT_TRUE(validateTexture(textures[4], false));
    EXPECT_TRUE(validateTexture(textures[7], false));

    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryForSelfManagedTextures());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, renderSurfacesReduceMemoryAvailableForRequestLate)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> renderSurfacePlaceHolder = resourceManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->SetToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->set_request_priority(PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set equal priorities.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100);

    // The first four to be requested late will be available.
    prioritizeTexturesAndBackings(resourceManager.get());
    for (unsigned i = 0; i < maxTextures; ++i)
        EXPECT_FALSE(validateTexture(textures[i], false));
    for (unsigned i = 0; i < maxTextures; i += 2)
        EXPECT_TRUE(validateTexture(textures[i], true));
    for (unsigned i = 1; i < maxTextures; i += 2)
        EXPECT_FALSE(validateTexture(textures[i], true));

    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryForSelfManagedTextures());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, whenRenderSurfaceNotAvailableTexturesAlsoNotAvailable)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> renderSurfacePlaceHolder = resourceManager->createTexture(m_textureSize, m_textureFormat);
    renderSurfacePlaceHolder->SetToSelfManagedMemoryPlaceholder(texturesMemorySize(4));
    renderSurfacePlaceHolder->set_request_priority(PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set 6 visible textures in the root surface, and 2 in a child surface.
    for (size_t i = 0; i < 6; ++i)
        textures[i]->set_request_priority(PriorityCalculator::VisiblePriority(true));
    for (size_t i = 6; i < 8; ++i)
        textures[i]->set_request_priority(PriorityCalculator::VisiblePriority(false));

    prioritizeTexturesAndBackings(resourceManager.get());

    // Unable to requestLate textures in the child surface.
    EXPECT_FALSE(validateTexture(textures[6], true));
    EXPECT_FALSE(validateTexture(textures[7], true));

    // Root surface textures are valid.
    for (size_t i = 0; i < 6; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));

    EXPECT_EQ(texturesMemorySize(6), resourceManager->memoryAboveCutoffBytes());
    EXPECT_EQ(texturesMemorySize(2), resourceManager->memoryForSelfManagedTextures());
    EXPECT_LE(resourceManager->memoryUseBytes(), resourceManager->memoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, requestLateBackingsSorting)
{
    const size_t maxTextures = 8;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(maxTextures));

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures];
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set equal priorities, and allocate backings for all textures.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100);
    prioritizeTexturesAndBackings(resourceManager.get());
    for (unsigned i = 0; i < maxTextures; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));

    // Drop the memory limit and prioritize (none will be above the threshold,
    // but they still have backings because reduceMemory hasn't been called).
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(maxTextures / 2));
    prioritizeTexturesAndBackings(resourceManager.get());

    // Push half of them back over the limit.
    for (size_t i = 0; i < maxTextures; i += 2)
        EXPECT_TRUE(textures[i]->RequestLate());

    // Push the priorities to the backings array and sort the backings array
    resourceManagerUpdateBackingsPriorities(resourceManager.get());

    // Assert that the backings list be sorted with the below-limit backings
    // before the above-limit backings.
    resourceManagerAssertInvariants(resourceManager.get());

    // Make sure that we have backings for all of the textures.
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_TRUE(textures[i]->have_backing_texture());

    // Make sure that only the requestLate textures are above the priority cutoff
    for (size_t i = 0; i < maxTextures; i += 2)
        EXPECT_TRUE(textureBackingIsAbovePriorityCutoff(textures[i].get()));
    for (size_t i = 1; i < maxTextures; i += 2)
        EXPECT_FALSE(textureBackingIsAbovePriorityCutoff(textures[i].get()));

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

TEST_F(PrioritizedResourceTest, clearUploadsToEvictedResources)
{
    const size_t maxTextures = 4;
    scoped_ptr<PrioritizedResourceManager> resourceManager =
        createManager(maxTextures);
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(maxTextures));

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    // Set equal priorities, and allocate backings for all textures.
    for (size_t i = 0; i < maxTextures; ++i)
        textures[i]->set_request_priority(100);
    prioritizeTexturesAndBackings(resourceManager.get());
    for (unsigned i = 0; i < maxTextures; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));

    ResourceUpdateQueue queue;
    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    for (size_t i = 0; i < maxTextures; ++i) {
        const ResourceUpdate upload = ResourceUpdate::Create(
            textures[i].get(), NULL, gfx::Rect(), gfx::Rect(), gfx::Vector2d());
        queue.appendFullUpload(upload);
    }

    // Make sure that we have backings for all of the textures.
    for (size_t i = 0; i < maxTextures; ++i)
        EXPECT_TRUE(textures[i]->have_backing_texture());

    queue.clearUploadsToEvictedResources();
    EXPECT_EQ(4, queue.fullUploadSize());

    resourceManager->reduceMemoryOnImplThread(
        texturesMemorySize(1), PriorityCalculator::AllowEverythingCutoff(), resourceProvider());
    queue.clearUploadsToEvictedResources();
    EXPECT_EQ(1, queue.fullUploadSize());

    resourceManager->reduceMemoryOnImplThread(0,  PriorityCalculator::AllowEverythingCutoff(), resourceProvider());
    queue.clearUploadsToEvictedResources();
    EXPECT_EQ(0, queue.fullUploadSize());

}

TEST_F(PrioritizedResourceTest, usageStatistics)
{
    const size_t maxTextures = 5;
    scoped_ptr<PrioritizedResourceManager> resourceManager = createManager(maxTextures);
    scoped_ptr<PrioritizedResource> textures[maxTextures];

    for (size_t i = 0; i < maxTextures; ++i)
        textures[i] = resourceManager->createTexture(m_textureSize, m_textureFormat);

    textures[0]->set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[1]->set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff());
    textures[2]->set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
    textures[3]->set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff());
    textures[4]->set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff() + 1);

    // Set max limit to 2 textures.
    resourceManager->setMaxMemoryLimitBytes(texturesMemorySize(2));
    prioritizeTexturesAndBackings(resourceManager.get());

    // The first two textures should be available, others should not.
    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE(validateTexture(textures[i], false));
    for (size_t i = 2; i < maxTextures; ++i)
        EXPECT_FALSE(validateTexture(textures[i], false));

    // Validate the statistics.
    {
        DebugScopedSetImplThread implThread(&m_proxy);
        EXPECT_EQ(texturesMemorySize(2), resourceManager->memoryUseBytes());
        EXPECT_EQ(texturesMemorySize(1), resourceManager->memoryVisibleBytes());
        EXPECT_EQ(texturesMemorySize(3), resourceManager->memoryVisibleAndNearbyBytes());
    }

    // Re-prioritize the textures, but do not push the values to backings.
    textures[0]->set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[1]->set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[2]->set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[3]->set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
    textures[4]->set_request_priority(PriorityCalculator::AllowVisibleAndNearbyCutoff());
    resourceManager->prioritizeTextures();

    // Verify that we still see the old values.
    {
        DebugScopedSetImplThread implThread(&m_proxy);
        EXPECT_EQ(texturesMemorySize(2), resourceManager->memoryUseBytes());
        EXPECT_EQ(texturesMemorySize(1), resourceManager->memoryVisibleBytes());
        EXPECT_EQ(texturesMemorySize(3), resourceManager->memoryVisibleAndNearbyBytes());
    }

    // Push priorities to backings, and verify we see the new values.
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
        resourceManager->pushTexturePrioritiesToBackings();
        EXPECT_EQ(texturesMemorySize(2), resourceManager->memoryUseBytes());
        EXPECT_EQ(texturesMemorySize(3), resourceManager->memoryVisibleBytes());
        EXPECT_EQ(texturesMemorySize(4), resourceManager->memoryVisibleAndNearbyBytes());
    }

    DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(&m_proxy);
    resourceManager->clearAllMemory(resourceProvider());
}

}  // namespace
}  // namespace cc
