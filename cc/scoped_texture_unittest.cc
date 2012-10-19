// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScopedTexture.h"

#include "CCRenderer.h"
#include "cc/single_thread_proxy.h" // For DebugScopedSetImplThread
#include "cc/test/fake_graphics_context.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;

namespace {

TEST(ScopedTextureTest, NewScopedTexture)
{
    scoped_ptr<GraphicsContext> context(createFakeGraphicsContext());
    DebugScopedSetImplThread implThread;
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));
    scoped_ptr<ScopedTexture> texture = ScopedTexture::create(resourceProvider.get());

    // New scoped textures do not hold a texture yet.
    EXPECT_EQ(0u, texture->id());

    // New scoped textures do not have a size yet.
    EXPECT_EQ(IntSize(), texture->size());
    EXPECT_EQ(0u, texture->bytes());
}

TEST(ScopedTextureTest, CreateScopedTexture)
{
    scoped_ptr<GraphicsContext> context(createFakeGraphicsContext());
    DebugScopedSetImplThread implThread;
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));
    scoped_ptr<ScopedTexture> texture = ScopedTexture::create(resourceProvider.get());
    texture->allocate(Renderer::ImplPool, IntSize(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);

    // The texture has an allocated byte-size now.
    size_t expectedBytes = 30 * 30 * 4;
    EXPECT_EQ(expectedBytes, texture->bytes());

    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(GL_RGBA, texture->format());
    EXPECT_EQ(IntSize(30, 30), texture->size());
}

TEST(ScopedTextureTest, ScopedTextureIsDeleted)
{
    scoped_ptr<GraphicsContext> context(createFakeGraphicsContext());
    DebugScopedSetImplThread implThread;
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));

    {
        scoped_ptr<ScopedTexture> texture = ScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(Renderer::ImplPool, IntSize(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(0u, resourceProvider->numResources());

    {
        scoped_ptr<ScopedTexture> texture = ScopedTexture::create(resourceProvider.get());
        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(Renderer::ImplPool, IntSize(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
        texture->free();
        EXPECT_EQ(0u, resourceProvider->numResources());
    }
}

TEST(ScopedTextureTest, LeakScopedTexture)
{
    scoped_ptr<GraphicsContext> context(createFakeGraphicsContext());
    DebugScopedSetImplThread implThread;
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::create(context.get()));

    {
        scoped_ptr<ScopedTexture> texture = ScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(Renderer::ImplPool, IntSize(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->leak();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->free();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(1u, resourceProvider->numResources());
}

}
