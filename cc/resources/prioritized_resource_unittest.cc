// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/prioritized_resource.h"

#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/single_thread_proxy.h"  // For DebugScopedSetImplThread
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class PrioritizedResourceTest : public testing::Test {
 public:
    PrioritizedResourceTest()
        : proxy_(scoped_ptr<Thread>(NULL)),
          texture_size_(256, 256),
          texture_format_(GL_RGBA),
          output_surface_(CreateFakeOutputSurface()) {
        DebugScopedSetImplThread impl_thread(&proxy_);
        resource_provider_ = cc::ResourceProvider::Create(output_surface_.get());
    }

    virtual ~PrioritizedResourceTest() {
        DebugScopedSetImplThread impl_thread(&proxy_);
        resource_provider_.reset();
    }

    size_t TexturesMemorySize(size_t texture_count) {
        return Resource::MemorySizeBytes(texture_size_, texture_format_) *
               texture_count;
    }

    scoped_ptr<PrioritizedResourceManager> CreateManager(size_t max_textures) {
        scoped_ptr<PrioritizedResourceManager> manager =
            PrioritizedResourceManager::Create(&proxy_);
        manager->SetMaxMemoryLimitBytes(TexturesMemorySize(max_textures));
        return manager.Pass();
    }

    bool ValidateTexture(scoped_ptr<PrioritizedResource>& texture,
                         bool request_late) {
        ResourceManagerAssertInvariants(texture->resource_manager());
        if (request_late)
            texture->RequestLate();
        ResourceManagerAssertInvariants(texture->resource_manager());
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        bool success = texture->can_acquire_backing_texture();
        if (success)
            texture->AcquireBackingTexture(ResourceProvider());
        return success;
    }

    void PrioritizeTexturesAndBackings(
        PrioritizedResourceManager* resource_manager) {
        resource_manager->PrioritizeTextures();
        ResourceManagerUpdateBackingsPriorities(resource_manager);
    }

    void ResourceManagerUpdateBackingsPriorities(
        PrioritizedResourceManager* resource_manager) {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->PushTexturePrioritiesToBackings();
    }

    cc::ResourceProvider* ResourceProvider() { return resource_provider_.get(); }

    void ResourceManagerAssertInvariants(
        PrioritizedResourceManager* resource_manager) {
#ifndef NDEBUG
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->AssertInvariants();
#endif
    }

    bool TextureBackingIsAbovePriorityCutoff(PrioritizedResource* texture) {
        return texture->backing()->
            was_above_priority_cutoff_at_last_priority_update();
    }

    size_t EvictedBackingCount(PrioritizedResourceManager* resource_manager) {
        return resource_manager->evicted_backings_.size();
    }

 protected:
    FakeProxy proxy_;
    const gfx::Size texture_size_;
    const GLenum texture_format_;
    scoped_ptr<OutputSurface> output_surface_;
    scoped_ptr<cc::ResourceProvider> resource_provider_;
};

namespace {

TEST_F(PrioritizedResourceTest, RequestTextureExceedingMaxLimit) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);

    // Create textures for double our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures * 2];

    for (size_t i = 0; i < max_textures * 2; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set decreasing priorities
    for (size_t i = 0; i < max_textures * 2; ++i)
        textures[i]->set_request_priority(100 + i);

    // Only lower half should be available.
    PrioritizeTexturesAndBackings(resource_manager.get());
    EXPECT_TRUE(ValidateTexture(textures[0], false));
    EXPECT_TRUE(ValidateTexture(textures[7], false));
    EXPECT_FALSE(ValidateTexture(textures[8], false));
    EXPECT_FALSE(ValidateTexture(textures[15], false));

    // Set increasing priorities
    for (size_t i = 0; i < max_textures * 2; ++i)
        textures[i]->set_request_priority(100 - i);

    // Only upper half should be available.
    PrioritizeTexturesAndBackings(resource_manager.get());
    EXPECT_FALSE(ValidateTexture(textures[0], false));
    EXPECT_FALSE(ValidateTexture(textures[7], false));
    EXPECT_TRUE(ValidateTexture(textures[8], false));
    EXPECT_TRUE(ValidateTexture(textures[15], false));

    EXPECT_EQ(TexturesMemorySize(max_textures),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ChangeMemoryLimits) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Set max limit to 8 textures
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        ValidateTexture(textures[i], false); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }

    EXPECT_EQ(TexturesMemorySize(8),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    // Set max limit to 5 textures
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(5));
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_EQ(ValidateTexture(textures[i], false), i < 5); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }

    EXPECT_EQ(TexturesMemorySize(5),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    // Set max limit to 4 textures
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(4));
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_EQ(ValidateTexture(textures[i], false), i < 4); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }

    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ChangePriorityCutoff) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Set the cutoff to drop two textures. Try to request_late on all textures,
    // and make sure that request_late doesn't work on a texture with equal
    // priority to the cutoff.
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
    resource_manager->SetExternalPriorityCutoff(106);
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_EQ(ValidateTexture(textures[i], true), i < 6); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }
    EXPECT_EQ(TexturesMemorySize(6),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    // Set the cutoff to drop two more textures.
    resource_manager->SetExternalPriorityCutoff(104);
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_EQ(ValidateTexture(textures[i], false), i < 4); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }
    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryAboveCutoffBytes());

    // Do a one-time eviction for one more texture based on priority cutoff
    PrioritizedResourceManager::BackingList evicted_backings;
    resource_manager->UnlinkAndClearEvictedBackings(); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemoryOnImplThread(
            TexturesMemorySize(8), 104, ResourceProvider());
        EXPECT_EQ(0, EvictedBackingCount(resource_manager.get()));
        resource_manager->ReduceMemoryOnImplThread(
            TexturesMemorySize(8), 103, ResourceProvider());
        EXPECT_EQ(1, EvictedBackingCount(resource_manager.get()));
    }
    resource_manager->UnlinkAndClearEvictedBackings();
    EXPECT_EQ(TexturesMemorySize(3), resource_manager->MemoryUseBytes());

    // Re-allocate the the texture after the one-time drop.
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_EQ(ValidateTexture(textures[i], false), i < 4); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ReduceMemory(ResourceProvider());
    }
    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerPartialUpdateTextures) {
    const size_t max_textures = 4;
    const size_t num_textures = 4;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    scoped_ptr<PrioritizedResource> textures[num_textures];
    scoped_ptr<PrioritizedResource> more_textures[num_textures];

    for (size_t i = 0; i < num_textures; ++i) {
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);
        more_textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);
    }

    for (size_t i = 0; i < num_textures; ++i)
        textures[i]->set_request_priority(200 + i);
    PrioritizeTexturesAndBackings(resource_manager.get());

    // Allocate textures which are currently high priority.
    EXPECT_TRUE(ValidateTexture(textures[0], false));
    EXPECT_TRUE(ValidateTexture(textures[1], false));
    EXPECT_TRUE(ValidateTexture(textures[2], false));
    EXPECT_TRUE(ValidateTexture(textures[3], false));

    EXPECT_TRUE(textures[0]->have_backing_texture());
    EXPECT_TRUE(textures[1]->have_backing_texture());
    EXPECT_TRUE(textures[2]->have_backing_texture());
    EXPECT_TRUE(textures[3]->have_backing_texture());

    for (size_t i = 0; i < num_textures; ++i)
        more_textures[i]->set_request_priority(100 + i);
    PrioritizeTexturesAndBackings(resource_manager.get());

    // Textures are now below cutoff.
    EXPECT_FALSE(ValidateTexture(textures[0], false));
    EXPECT_FALSE(ValidateTexture(textures[1], false));
    EXPECT_FALSE(ValidateTexture(textures[2], false));
    EXPECT_FALSE(ValidateTexture(textures[3], false));

    // But they are still valid to use.
    EXPECT_TRUE(textures[0]->have_backing_texture());
    EXPECT_TRUE(textures[1]->have_backing_texture());
    EXPECT_TRUE(textures[2]->have_backing_texture());
    EXPECT_TRUE(textures[3]->have_backing_texture());

    // Higher priority textures are finally needed.
    EXPECT_TRUE(ValidateTexture(more_textures[0], false));
    EXPECT_TRUE(ValidateTexture(more_textures[1], false));
    EXPECT_TRUE(ValidateTexture(more_textures[2], false));
    EXPECT_TRUE(ValidateTexture(more_textures[3], false));

    // Lower priority have been fully evicted.
    EXPECT_FALSE(textures[0]->have_backing_texture());
    EXPECT_FALSE(textures[1]->have_backing_texture());
    EXPECT_FALSE(textures[2]->have_backing_texture());
    EXPECT_FALSE(textures[3]->have_backing_texture());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerPrioritiesAreEqual) {
    const size_t max_textures = 16;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // All 16 textures have the same priority except 2 higher priority.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100);
    textures[0]->set_request_priority(99);
    textures[1]->set_request_priority(99);

    // Set max limit to 8 textures
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(8));
    PrioritizeTexturesAndBackings(resource_manager.get());

    // The two high priority textures should be available, others should not.
    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE(ValidateTexture(textures[i], false));
    for (size_t i = 2; i < max_textures; ++i)
        EXPECT_FALSE(ValidateTexture(textures[i], false));
    EXPECT_EQ(TexturesMemorySize(2),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    // Manually reserving textures should only succeed on the higher priority
    // textures, and on remaining textures up to the memory limit.
    for (size_t i = 0; i < 8; i++)
        EXPECT_TRUE(ValidateTexture(textures[i], true));
    for (size_t i = 9; i < max_textures; i++)
        EXPECT_FALSE(ValidateTexture(textures[i], true));
    EXPECT_EQ(TexturesMemorySize(8),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ResourceManagerDestroyedFirst) {
    scoped_ptr<PrioritizedResourceManager> resource_manager = CreateManager(1);
    scoped_ptr<PrioritizedResource> texture =
        resource_manager->CreateTexture(texture_size_, texture_format_);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->have_backing_texture());

    texture->set_request_priority(100);
    PrioritizeTexturesAndBackings(resource_manager.get());

    EXPECT_TRUE(ValidateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture()); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager->ClearAllMemory(ResourceProvider());
    }
    resource_manager.reset();

    EXPECT_FALSE(texture->can_acquire_backing_texture());
    EXPECT_FALSE(texture->have_backing_texture());
}

TEST_F(PrioritizedResourceTest, TextureMovedToNewManager) {
    scoped_ptr<PrioritizedResourceManager> resource_manager_one =
        CreateManager(1);
    scoped_ptr<PrioritizedResourceManager> resource_manager_two =
        CreateManager(1);
    scoped_ptr<PrioritizedResource> texture =
        resource_manager_one->CreateTexture(texture_size_, texture_format_);

    // Texture is initially invalid, but it will become available.
    EXPECT_FALSE(texture->have_backing_texture());

    texture->set_request_priority(100);
    PrioritizeTexturesAndBackings(resource_manager_one.get());

    EXPECT_TRUE(ValidateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture());

    texture->SetTextureManager(0); {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked(&proxy_);
        resource_manager_one->ClearAllMemory(ResourceProvider());
    }
    resource_manager_one.reset();

    EXPECT_FALSE(texture->can_acquire_backing_texture());
    EXPECT_FALSE(texture->have_backing_texture());

    texture->SetTextureManager(resource_manager_two.get());

    PrioritizeTexturesAndBackings(resource_manager_two.get());

    EXPECT_TRUE(ValidateTexture(texture, false));
    EXPECT_TRUE(texture->can_acquire_backing_texture());
    EXPECT_TRUE(texture->have_backing_texture());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager_two->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest,
       RenderSurfacesReduceMemoryAvailableOutsideRootSurface) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> render_surface_place_holder =
        resource_manager->CreateTexture(texture_size_, texture_format_);
    render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
        TexturesMemorySize(4));
    render_surface_place_holder->set_request_priority(
        PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set decreasing non-visible priorities outside root surface.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100 + i);

    // Only lower half should be available.
    PrioritizeTexturesAndBackings(resource_manager.get());
    EXPECT_TRUE(ValidateTexture(textures[0], false));
    EXPECT_TRUE(ValidateTexture(textures[3], false));
    EXPECT_FALSE(ValidateTexture(textures[4], false));
    EXPECT_FALSE(ValidateTexture(textures[7], false));

    // Set increasing non-visible priorities outside root surface.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100 - i);

    // Only upper half should be available.
    PrioritizeTexturesAndBackings(resource_manager.get());
    EXPECT_FALSE(ValidateTexture(textures[0], false));
    EXPECT_FALSE(ValidateTexture(textures[3], false));
    EXPECT_TRUE(ValidateTexture(textures[4], false));
    EXPECT_TRUE(ValidateTexture(textures[7], false));

    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryForSelfManagedTextures());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest,
       RenderSurfacesReduceMemoryAvailableForRequestLate) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> render_surface_place_holder =
        resource_manager->CreateTexture(texture_size_, texture_format_);
    render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
        TexturesMemorySize(4));
    render_surface_place_holder->set_request_priority(
        PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set equal priorities.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100);

    // The first four to be requested late will be available.
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (unsigned i = 0; i < max_textures; ++i)
        EXPECT_FALSE(ValidateTexture(textures[i], false));
    for (unsigned i = 0; i < max_textures; i += 2)
        EXPECT_TRUE(ValidateTexture(textures[i], true));
    for (unsigned i = 1; i < max_textures; i += 2)
        EXPECT_FALSE(ValidateTexture(textures[i], true));

    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_EQ(TexturesMemorySize(4),
              resource_manager->MemoryForSelfManagedTextures());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest,
       WhenRenderSurfaceNotAvailableTexturesAlsoNotAvailable) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);

    // Half of the memory is taken by surfaces (with high priority place-holder)
    scoped_ptr<PrioritizedResource> render_surface_place_holder =
        resource_manager->CreateTexture(texture_size_, texture_format_);
    render_surface_place_holder->SetToSelfManagedMemoryPlaceholder(
        TexturesMemorySize(4));
    render_surface_place_holder->set_request_priority(
        PriorityCalculator::RenderSurfacePriority());

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set 6 visible textures in the root surface, and 2 in a child surface.
    for (size_t i = 0; i < 6; ++i)
        textures[i]->
            set_request_priority(PriorityCalculator::VisiblePriority(true));
    for (size_t i = 6; i < 8; ++i)
        textures[i]->
            set_request_priority(PriorityCalculator::VisiblePriority(false));

    PrioritizeTexturesAndBackings(resource_manager.get());

    // Unable to request_late textures in the child surface.
    EXPECT_FALSE(ValidateTexture(textures[6], true));
    EXPECT_FALSE(ValidateTexture(textures[7], true));

    // Root surface textures are valid.
    for (size_t i = 0; i < 6; ++i)
        EXPECT_TRUE(ValidateTexture(textures[i], false));

    EXPECT_EQ(TexturesMemorySize(6),
              resource_manager->MemoryAboveCutoffBytes());
    EXPECT_EQ(TexturesMemorySize(2),
              resource_manager->MemoryForSelfManagedTextures());
    EXPECT_LE(resource_manager->MemoryUseBytes(),
              resource_manager->MemoryAboveCutoffBytes());

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, RequestLateBackingsSorting) {
    const size_t max_textures = 8;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(max_textures));

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures];
    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set equal priorities, and allocate backings for all textures.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100);
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (unsigned i = 0; i < max_textures; ++i)
        EXPECT_TRUE(ValidateTexture(textures[i], false));

    // Drop the memory limit and prioritize (none will be above the threshold,
    // but they still have backings because reduceMemory hasn't been called).
    resource_manager->SetMaxMemoryLimitBytes(
        TexturesMemorySize(max_textures / 2));
    PrioritizeTexturesAndBackings(resource_manager.get());

    // Push half of them back over the limit.
    for (size_t i = 0; i < max_textures; i += 2)
        EXPECT_TRUE(textures[i]->RequestLate());

    // Push the priorities to the backings array and sort the backings array
    ResourceManagerUpdateBackingsPriorities(resource_manager.get());

    // Assert that the backings list be sorted with the below-limit backings
    // before the above-limit backings.
    ResourceManagerAssertInvariants(resource_manager.get());

    // Make sure that we have backings for all of the textures.
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_TRUE(textures[i]->have_backing_texture());

    // Make sure that only the request_late textures are above the priority
    // cutoff
    for (size_t i = 0; i < max_textures; i += 2)
        EXPECT_TRUE(TextureBackingIsAbovePriorityCutoff(textures[i].get()));
    for (size_t i = 1; i < max_textures; i += 2)
        EXPECT_FALSE(TextureBackingIsAbovePriorityCutoff(textures[i].get()));

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

TEST_F(PrioritizedResourceTest, ClearUploadsToEvictedResources) {
    const size_t max_textures = 4;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(max_textures));

    // Create textures to fill our memory limit.
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    // Set equal priorities, and allocate backings for all textures.
    for (size_t i = 0; i < max_textures; ++i)
        textures[i]->set_request_priority(100);
    PrioritizeTexturesAndBackings(resource_manager.get());
    for (unsigned i = 0; i < max_textures; ++i)
        EXPECT_TRUE(ValidateTexture(textures[i], false));

    ResourceUpdateQueue queue;
    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked(&proxy_);
    for (size_t i = 0; i < max_textures; ++i) {
        const ResourceUpdate upload = ResourceUpdate::Create(
            textures[i].get(), NULL, gfx::Rect(), gfx::Rect(), gfx::Vector2d());
        queue.AppendFullUpload(upload);
    }

    // Make sure that we have backings for all of the textures.
    for (size_t i = 0; i < max_textures; ++i)
        EXPECT_TRUE(textures[i]->have_backing_texture());

    queue.ClearUploadsToEvictedResources();
    EXPECT_EQ(4, queue.FullUploadSize());

    resource_manager->ReduceMemoryOnImplThread(
        TexturesMemorySize(1),
        PriorityCalculator::AllowEverythingCutoff(),
        ResourceProvider());
    queue.ClearUploadsToEvictedResources();
    EXPECT_EQ(1, queue.FullUploadSize());

    resource_manager->ReduceMemoryOnImplThread(
        0, PriorityCalculator::AllowEverythingCutoff(), ResourceProvider());
    queue.ClearUploadsToEvictedResources();
    EXPECT_EQ(0, queue.FullUploadSize());
}

TEST_F(PrioritizedResourceTest, UsageStatistics) {
    const size_t max_textures = 5;
    scoped_ptr<PrioritizedResourceManager> resource_manager =
        CreateManager(max_textures);
    scoped_ptr<PrioritizedResource> textures[max_textures];

    for (size_t i = 0; i < max_textures; ++i)
        textures[i] =
            resource_manager->CreateTexture(texture_size_, texture_format_);

    textures[0]->set_request_priority(
        PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[1]->
        set_request_priority(PriorityCalculator::AllowVisibleOnlyCutoff());
    textures[2]->set_request_priority(
        PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
    textures[3]->set_request_priority(
        PriorityCalculator::AllowVisibleAndNearbyCutoff());
    textures[4]->set_request_priority(
        PriorityCalculator::AllowVisibleAndNearbyCutoff() + 1);

    // Set max limit to 2 textures.
    resource_manager->SetMaxMemoryLimitBytes(TexturesMemorySize(2));
    PrioritizeTexturesAndBackings(resource_manager.get());

    // The first two textures should be available, others should not.
    for (size_t i = 0; i < 2; ++i)
        EXPECT_TRUE(ValidateTexture(textures[i], false));
    for (size_t i = 2; i < max_textures; ++i)
        EXPECT_FALSE(ValidateTexture(textures[i], false));

    // Validate the statistics.
    {
        DebugScopedSetImplThread impl_thread(&proxy_);
        EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
        EXPECT_EQ(TexturesMemorySize(1),
                  resource_manager->MemoryVisibleBytes());
        EXPECT_EQ(TexturesMemorySize(3),
                  resource_manager->MemoryVisibleAndNearbyBytes());
    }

    // Re-prioritize the textures, but do not push the values to backings.
    textures[0]->set_request_priority(
        PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[1]->set_request_priority(
        PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[2]->set_request_priority(
        PriorityCalculator::AllowVisibleOnlyCutoff() - 1);
    textures[3]->set_request_priority(
        PriorityCalculator::AllowVisibleAndNearbyCutoff() - 1);
    textures[4]->set_request_priority(
        PriorityCalculator::AllowVisibleAndNearbyCutoff());
    resource_manager->PrioritizeTextures();

    // Verify that we still see the old values.
    {
        DebugScopedSetImplThread impl_thread (& proxy_);
        EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
        EXPECT_EQ(TexturesMemorySize(1),
                  resource_manager->MemoryVisibleBytes());
        EXPECT_EQ(TexturesMemorySize(3),
                  resource_manager->MemoryVisibleAndNearbyBytes());
    }

    // Push priorities to backings, and verify we see the new values.
    {
        DebugScopedSetImplThreadAndMainThreadBlocked
        impl_thread_and_main_thread_blocked (& proxy_);
        resource_manager->PushTexturePrioritiesToBackings();
        EXPECT_EQ(TexturesMemorySize(2), resource_manager->MemoryUseBytes());
        EXPECT_EQ(TexturesMemorySize(3),
                  resource_manager->MemoryVisibleBytes());
        EXPECT_EQ(TexturesMemorySize(4),
                  resource_manager->MemoryVisibleAndNearbyBytes());
    }

    DebugScopedSetImplThreadAndMainThreadBlocked
    impl_thread_and_main_thread_blocked (& proxy_);
    resource_manager->ClearAllMemory(ResourceProvider());
}

}  // namespace
}  // namespace cc
