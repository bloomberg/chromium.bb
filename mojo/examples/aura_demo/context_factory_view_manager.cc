// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/aura_demo/context_factory_view_manager.h"

#include "base/bind.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace examples {
namespace {

void FreeSharedBitmap(cc::SharedBitmap* shared_bitmap) {
  delete shared_bitmap->memory();
}

void IgnoreSharedBitmap(cc::SharedBitmap* shared_bitmap) {}

bool CreateMapAndDupSharedBuffer(size_t size,
                                 void** memory,
                                 ScopedSharedBufferHandle* handle,
                                 ScopedSharedBufferHandle* duped) {
  MojoResult result = CreateSharedBuffer(NULL, size, handle);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(handle->is_valid());

  result = DuplicateBuffer(handle->get(), NULL, duped);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(duped->is_valid());

  result = MapBuffer(
      handle->get(), 0, size, memory, MOJO_MAP_BUFFER_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(*memory);

  return true;
}

class SoftwareOutputDeviceViewManager : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceViewManager(
      view_manager::IViewManager* view_manager,
      uint32_t view_id)
      : view_manager_(view_manager),
        view_id_(view_id) {
  }
  virtual ~SoftwareOutputDeviceViewManager() {}

  // cc::SoftwareOutputDevice:
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE {
    SetViewContents();

    SoftwareOutputDevice::EndPaint(frame_data);
  }

 private:
  void OnSetViewContentsDone(bool value) {
    VLOG(1) << "SoftwareOutputDeviceManager::OnSetViewContentsDone " << value;
    DCHECK(value);
  }

  void SetViewContents() {
    std::vector<unsigned char> data;
    gfx::PNGCodec::EncodeBGRASkBitmap(
        skia::GetTopDevice(*canvas_)->accessBitmap(true), false, &data);

    void* memory = NULL;
    ScopedSharedBufferHandle duped;
    bool result = CreateMapAndDupSharedBuffer(data.size(),
                                              &memory,
                                              &shared_state_handle_,
                                              &duped);
    if (!result)
      return;

    memcpy(memory, &data[0], data.size());

    view_manager_->SetViewContents(
        view_id_, duped.Pass(), static_cast<uint32_t>(data.size()),
        base::Bind(&SoftwareOutputDeviceViewManager::OnSetViewContentsDone,
                   base::Unretained(this)));
  }

  view_manager::IViewManager* view_manager_;
  const uint32_t view_id_;
  ScopedSharedBufferHandle shared_state_handle_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceViewManager);
};

// TODO(sky): this is a copy from cc/test. Copy to a common place.
class TestSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  TestSharedBitmapManager() {}
  virtual ~TestSharedBitmapManager() {}

  virtual scoped_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) OVERRIDE {
    base::AutoLock lock(lock_);
    scoped_ptr<base::SharedMemory> memory(new base::SharedMemory);
    memory->CreateAndMapAnonymous(size.GetArea() * 4);
    cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
    bitmap_map_[id] = memory.get();
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(memory.release(), id,
                             base::Bind(&FreeSharedBitmap)));
  }

  virtual scoped_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const cc::SharedBitmapId& id) OVERRIDE {
    base::AutoLock lock(lock_);
    if (bitmap_map_.find(id) == bitmap_map_.end())
      return scoped_ptr<cc::SharedBitmap>();
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(bitmap_map_[id], id,
                             base::Bind(&IgnoreSharedBitmap)));
  }

  virtual scoped_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* memory) OVERRIDE {
    base::AutoLock lock(lock_);
    cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
    bitmap_map_[id] = memory;
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(memory, id, base::Bind(&IgnoreSharedBitmap)));
  }

 private:
  base::Lock lock_;
  std::map<cc::SharedBitmapId, base::SharedMemory*> bitmap_map_;

  DISALLOW_COPY_AND_ASSIGN(TestSharedBitmapManager);
};

}  // namespace

ContextFactoryViewManager::ContextFactoryViewManager(
    view_manager::IViewManager* view_manager,
    uint32_t view_id)
    : view_manager_(view_manager),
      view_id_(view_id),
      shared_bitmap_manager_(new TestSharedBitmapManager()) {
}

ContextFactoryViewManager::~ContextFactoryViewManager() {}

scoped_ptr<cc::OutputSurface> ContextFactoryViewManager::CreateOutputSurface(
    ui::Compositor* compositor,
    bool software_fallback) {
  scoped_ptr<cc::SoftwareOutputDevice> output_device(
      new SoftwareOutputDeviceViewManager(view_manager_, view_id_));
  return make_scoped_ptr(new cc::OutputSurface(output_device.Pass()));
}

scoped_refptr<ui::Reflector> ContextFactoryViewManager::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  return new ui::Reflector();
}

void ContextFactoryViewManager::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
ContextFactoryViewManager::SharedMainThreadContextProvider() {
  return scoped_refptr<cc::ContextProvider>(NULL);
}

void ContextFactoryViewManager::RemoveCompositor(ui::Compositor* compositor) {}

bool ContextFactoryViewManager::DoesCreateTestContexts() { return false; }

cc::SharedBitmapManager* ContextFactoryViewManager::GetSharedBitmapManager() {
  return shared_bitmap_manager_.get();
}

base::MessageLoopProxy* ContextFactoryViewManager::GetCompositorMessageLoop() {
  return NULL;
}

}  // namespace examples
}  // namespace mojo
