// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_STREAM_TEXTURE_MANAGER_IN_PROCESS_ANDROID_H_
#define GPU_STREAM_TEXTURE_MANAGER_IN_PROCESS_ANDROID_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "gpu/command_buffer/service/stream_texture_manager.h"

namespace gfx {
class Size;
class SurfaceTexture;
}

namespace gpu {

class StreamTextureManagerInProcess
    : public gpu::StreamTextureManager,
      public base::RefCountedThreadSafe<StreamTextureManagerInProcess> {
 public:
  StreamTextureManagerInProcess();

  // implement gpu::StreamTextureManager:
  virtual uint32 CreateStreamTexture(uint32 service_id,
                                     uint32 client_id) OVERRIDE;
  virtual void DestroyStreamTexture(uint32 service_id) OVERRIDE;
  virtual gpu::StreamTexture* LookupStreamTexture(uint32 service_id) OVERRIDE;

  scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(uint32 stream_id);

 private:
  class StreamTextureImpl : public gpu::StreamTexture {
   public:
    StreamTextureImpl(uint32 service_id, uint32 stream_id);
    virtual ~StreamTextureImpl();

    // implement gpu::StreamTexture
    virtual void Update() OVERRIDE;
    virtual gfx::Size GetSize() OVERRIDE;

    void SetSize(gfx::Size size);

    scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture();
    uint32 stream_id() { return stream_id_; }

   private:
    scoped_refptr<gfx::SurfaceTexture> surface_texture_;
    uint32 stream_id_;
    gfx::Size size_;

    DISALLOW_COPY_AND_ASSIGN(StreamTextureImpl);
  };

  friend class base::RefCountedThreadSafe<StreamTextureManagerInProcess>;
  virtual ~StreamTextureManagerInProcess();

  typedef std::map<uint32, linked_ptr<StreamTextureImpl> > TextureMap;
  TextureMap textures_;

  uint32 next_id_;

  base::Lock map_lock_;

  DISALLOW_COPY_AND_ASSIGN(StreamTextureManagerInProcess);
};

}  // gpu

#endif  // GPU_STREAM_TEXTURE_MANAGER_IN_PROCESS_ANDROID_H_
