// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/scoped_resource.h"

#include "cc/output/renderer.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {
namespace {

TEST(ScopedResourceTest, NewScopedResource) {
  scoped_ptr<OutputSurface> context(CreateFakeOutputSurface());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(context.get()));
  scoped_ptr<ScopedResource> texture =
      ScopedResource::create(resource_provider.get());

  // New scoped textures do not hold a texture yet.
  EXPECT_EQ(0u, texture->id());

  // New scoped textures do not have a size yet.
  EXPECT_EQ(gfx::Size(), texture->size());
  EXPECT_EQ(0u, texture->bytes());
}

TEST(ScopedResourceTest, CreateScopedResource) {
  scoped_ptr<OutputSurface> context(CreateFakeOutputSurface());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(context.get()));
  scoped_ptr<ScopedResource> texture =
      ScopedResource::create(resource_provider.get());
  texture->Allocate(
      gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);

  // The texture has an allocated byte-size now.
  size_t expected_bytes = 30 * 30 * 4;
  EXPECT_EQ(expected_bytes, texture->bytes());

  EXPECT_LT(0u, texture->id());
  EXPECT_EQ(GL_RGBA, texture->format());
  EXPECT_EQ(gfx::Size(30, 30), texture->size());
}

TEST(ScopedResourceTest, ScopedResourceIsDeleted) {
  scoped_ptr<OutputSurface> context(CreateFakeOutputSurface());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(context.get()));
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::create(resource_provider.get());

    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(
        gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
  }

  EXPECT_EQ(0u, resource_provider->num_resources());
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::create(resource_provider.get());
    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(
        gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
    texture->Free();
    EXPECT_EQ(0u, resource_provider->num_resources());
  }
}

TEST(ScopedResourceTest, LeakScopedResource) {
  scoped_ptr<OutputSurface> context(CreateFakeOutputSurface());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(context.get()));
  {
    scoped_ptr<ScopedResource> texture =
        ScopedResource::create(resource_provider.get());

    EXPECT_EQ(0u, resource_provider->num_resources());
    texture->Allocate(
        gfx::Size(30, 30), GL_RGBA, ResourceProvider::TextureUsageAny);
    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());

    texture->Leak();
    EXPECT_EQ(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());

    texture->Free();
    EXPECT_EQ(0u, texture->id());
    EXPECT_EQ(1u, resource_provider->num_resources());
  }

  EXPECT_EQ(1u, resource_provider->num_resources());
}

}  // namespace
}  // namespace cc
