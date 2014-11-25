// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#include <map>
#endif

#include "base/bind.h"
#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/memory/linked_ptr.h"
#include "content/common/android/surface_texture_manager.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#endif

namespace content {
namespace {

#if defined(OS_ANDROID)
class SurfaceTextureManagerImpl : public SurfaceTextureManager {
 public:
  // Overridden from SurfaceTextureManager:
  void RegisterSurfaceTexture(int surface_texture_id,
                              int client_id,
                              gfx::SurfaceTexture* surface_texture) override {
    surfaces_[surface_texture_id] = make_linked_ptr(
        new gfx::ScopedJavaSurface(surface_texture));
  }
  void UnregisterSurfaceTexture(int surface_texture_id,
                                int client_id) override {
    surfaces_.erase(surface_texture_id);
  }
  gfx::AcceleratedWidget AcquireNativeWidgetForSurfaceTexture(
      int surface_texture_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    return ANativeWindow_fromSurface(
        env, surfaces_[surface_texture_id]->j_surface().obj());
  }

 private:
  typedef std::map<int, linked_ptr<gfx::ScopedJavaSurface>> SurfaceMap;
  SurfaceMap surfaces_;
};
#endif

class GpuMemoryBufferImplTest
    : public testing::TestWithParam<gfx::GpuMemoryBufferType>,
      public GpuMemoryBufferFactoryHost {
 public:
  GpuMemoryBufferImplTest() : factory_(GpuMemoryBufferFactory::Create()) {
#if defined(OS_ANDROID)
    SurfaceTextureManager::InitInstance(&surface_texture_manager_);
#endif
  }
  ~GpuMemoryBufferImplTest() override {
#if defined(OS_ANDROID)
    SurfaceTextureManager::InitInstance(NULL);
#endif
  }

  // Overridden from GpuMemoryBufferFactoryHost:
  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferType type,
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage,
      int client_id,
      const CreateGpuMemoryBufferCallback& callback) override {
    callback.Run(factory_->CreateGpuMemoryBuffer(type,
                                                 id,
                                                 size,
                                                 format,
                                                 usage,
                                                 client_id));
  }
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferType type,
                              gfx::GpuMemoryBufferId id,
                              int client_id,
                              int32 sync_point) override {
    factory_->DestroyGpuMemoryBuffer(type, id, client_id);
  }

 private:
  scoped_ptr<GpuMemoryBufferFactory> factory_;
#if defined(OS_ANDROID)
  SurfaceTextureManagerImpl surface_texture_manager_;
#endif
};

struct CreateRequest {
  scoped_ptr<GpuMemoryBufferImpl> result;
};

void GpuMemoryBufferCreated(CreateRequest* request,
                            scoped_ptr<GpuMemoryBufferImpl> buffer) {
  request->result = buffer.Pass();
}

TEST_P(GpuMemoryBufferImplTest, Create) {
  const int kBufferId = 1;
  const int kClientId = 0;

  gfx::Size buffer_size(1, 1);

  const gfx::GpuMemoryBuffer::Format formats[] = {
    gfx::GpuMemoryBuffer::RGBA_8888,
    gfx::GpuMemoryBuffer::RGBX_8888,
    gfx::GpuMemoryBuffer::BGRA_8888
  };
  for (gfx::GpuMemoryBuffer::Format format : formats) {
    const gfx::GpuMemoryBuffer::Usage usages[] = {
      gfx::GpuMemoryBuffer::MAP,
      gfx::GpuMemoryBuffer::SCANOUT
    };
    for (gfx::GpuMemoryBuffer::Usage usage : usages) {
      if (!GpuMemoryBufferImpl::IsConfigurationSupported(
            GetParam(), format, usage))
        continue;

      CreateRequest request;
      GpuMemoryBufferImpl::Create(GetParam(),
                                  kBufferId,
                                  buffer_size,
                                  format,
                                  usage,
                                  kClientId,
                                  base::Bind(GpuMemoryBufferCreated, &request));

      ASSERT_TRUE(request.result);

      scoped_ptr<GpuMemoryBufferImpl> buffer = request.result.Pass();
      EXPECT_EQ(buffer->GetFormat(), format);
    }
  }
}

struct AllocateRequest {
  gfx::GpuMemoryBufferHandle result;
};

void GpuMemoryBufferAllocated(AllocateRequest* request,
                              const gfx::GpuMemoryBufferHandle& handle) {
  request->result = handle;
}

void DeletedGpuMemoryBuffer(gfx::GpuMemoryBufferType type,
                            gfx::GpuMemoryBufferId id,
                            int child_client_id,
                            uint32 sync_point) {
  GpuMemoryBufferImpl::DeletedByChildProcess(type,
                                             id,
                                             base::GetCurrentProcessHandle(),
                                             child_client_id,
                                             sync_point);
}

TEST_P(GpuMemoryBufferImplTest, AllocateForChildProcess) {
  const int kBufferId = 1;
  const int kChildClientId = 1;

  gfx::Size buffer_size(1, 1);

  const gfx::GpuMemoryBuffer::Format formats[] = {
    gfx::GpuMemoryBuffer::RGBA_8888,
    gfx::GpuMemoryBuffer::RGBX_8888,
    gfx::GpuMemoryBuffer::BGRA_8888
  };
  for (gfx::GpuMemoryBuffer::Format format : formats) {
    const gfx::GpuMemoryBuffer::Usage usages[] = {
      gfx::GpuMemoryBuffer::MAP,
      gfx::GpuMemoryBuffer::SCANOUT
    };
    for (gfx::GpuMemoryBuffer::Usage usage : usages) {
      if (!GpuMemoryBufferImpl::IsConfigurationSupported(
            GetParam(), format, usage))
        continue;

      AllocateRequest request;
      GpuMemoryBufferImpl::AllocateForChildProcess(
          GetParam(),
          kBufferId,
          buffer_size,
          format,
          usage,
          base::GetCurrentProcessHandle(),
          kChildClientId,
          base::Bind(GpuMemoryBufferAllocated, &request));

      EXPECT_EQ(request.result.type, GetParam());

      scoped_ptr<GpuMemoryBufferImpl> buffer(
          GpuMemoryBufferImpl::CreateFromHandle(
              request.result,
              buffer_size,
              format,
              base::Bind(DeletedGpuMemoryBuffer,
                         GetParam(),
                         kBufferId,
                         kChildClientId)));
      ASSERT_TRUE(buffer);
      EXPECT_EQ(buffer->GetFormat(), format);
    }
  }
}

TEST_P(GpuMemoryBufferImplTest, Map) {
  const int kBufferId = 1;
  const int kChildClientId = 1;

  gfx::Size buffer_size(1, 1);

  const gfx::GpuMemoryBuffer::Format formats[] = {
    gfx::GpuMemoryBuffer::RGBA_8888,
    gfx::GpuMemoryBuffer::RGBX_8888,
    gfx::GpuMemoryBuffer::BGRA_8888
  };
  for (gfx::GpuMemoryBuffer::Format format : formats) {
    if (!GpuMemoryBufferImpl::IsConfigurationSupported(
            GetParam(), format, gfx::GpuMemoryBuffer::MAP))
      continue;

    size_t width_in_bytes =
        GpuMemoryBufferImpl::BytesPerPixel(format) * buffer_size.width();
    EXPECT_GT(width_in_bytes, 0u);
    scoped_ptr<char[]> data(new char[width_in_bytes]);
    memset(data.get(), 0x2a, width_in_bytes);

    AllocateRequest request;
    GpuMemoryBufferImpl::AllocateForChildProcess(
        GetParam(),
        kBufferId,
        buffer_size,
        format,
        gfx::GpuMemoryBuffer::MAP,
        base::GetCurrentProcessHandle(),
        kChildClientId,
        base::Bind(GpuMemoryBufferAllocated, &request));

    EXPECT_EQ(request.result.type, GetParam());

    scoped_ptr<GpuMemoryBufferImpl> buffer(
        GpuMemoryBufferImpl::CreateFromHandle(
            request.result,
            buffer_size,
            format,
            base::Bind(DeletedGpuMemoryBuffer,
                       GetParam(),
                       kBufferId,
                       kChildClientId)));
    ASSERT_TRUE(buffer);
    EXPECT_FALSE(buffer->IsMapped());

    void* memory = buffer->Map();
    ASSERT_TRUE(memory);
    EXPECT_TRUE(buffer->IsMapped());
    uint32 stride = buffer->GetStride();
    EXPECT_GE(stride, width_in_bytes);
    memcpy(memory, data.get(), width_in_bytes);
    EXPECT_EQ(memcmp(memory, data.get(), width_in_bytes), 0);
    buffer->Unmap();
    EXPECT_FALSE(buffer->IsMapped());
  }
}

std::vector<gfx::GpuMemoryBufferType> GetSupportedGpuMemoryBufferTypes() {
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferImpl::GetSupportedTypes(&supported_types);
  return supported_types;
}

INSTANTIATE_TEST_CASE_P(
    GpuMemoryBufferImplTests,
    GpuMemoryBufferImplTest,
    ::testing::ValuesIn(GetSupportedGpuMemoryBufferTypes()));

}  // namespace
}  // namespace content
