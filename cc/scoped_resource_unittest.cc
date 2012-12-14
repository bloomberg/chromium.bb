// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scoped_resource.h"

#include "cc/renderer.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

using namespace WebKit;

namespace cc {
namespace {

TEST(ScopedResourceTest, NewScopedResource)
{
    scoped_ptr<OutputSurface> context(createFakeOutputSurface());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));
    scoped_ptr<ScopedResource> texture = ScopedResource::create(resourceProvider.get());

    // New scoped textures do not hold a texture yet.
    EXPECT_EQ(0u, texture->id());

    // New scoped textures do not have a size yet.
    EXPECT_EQ(gfx::Size(), texture->size());
    EXPECT_EQ(0u, texture->bytes());
}

TEST(ScopedResourceTest, CreateScopedResource)
{
    scoped_ptr<OutputSurface> context(createFakeOutputSurface());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));
    scoped_ptr<ScopedResource> texture = ScopedResource::create(resourceProvider.get());
    texture->Allocate(gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);

    // The texture has an allocated byte-size now.
    size_t expectedBytes = 30 * 30 * 4;
    EXPECT_EQ(expectedBytes, texture->bytes());

    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(GL_RGBA, texture->format());
    EXPECT_EQ(gfx::Size(30, 30), texture->size());
}

TEST(ScopedResourceTest, ScopedResourceIsDeleted)
{
    scoped_ptr<OutputSurface> context(createFakeOutputSurface());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));

    {
        scoped_ptr<ScopedResource> texture = ScopedResource::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->Allocate(gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(0u, resourceProvider->numResources());

    {
        scoped_ptr<ScopedResource> texture = ScopedResource::create(resourceProvider.get());
        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->Allocate(gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
        texture->Free();
        EXPECT_EQ(0u, resourceProvider->numResources());
    }
}

TEST(ScopedResourceTest, LeakScopedResource)
{
    scoped_ptr<OutputSurface> context(createFakeOutputSurface());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));

    {
        scoped_ptr<ScopedResource> texture = ScopedResource::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->Allocate(gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->Leak();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->Free();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(1u, resourceProvider->numResources());
}

}  // namespace
}  // namespace cc
