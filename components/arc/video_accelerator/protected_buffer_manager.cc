// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/protected_buffer_manager.h"

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/sys_info.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace arc {

namespace {
// Size of the pixmap to be used as the dummy handle for protected buffers.
constexpr gfx::Size kDummyBufferSize(32, 32);
}  // namespace

ProtectedBufferHandle::ProtectedBufferHandle(
    base::OnceClosure destruction_cb,
    const base::SharedMemoryHandle& shm_handle)
    : shm_handle_(shm_handle), destruction_cb_(std::move(destruction_cb)) {
  DCHECK(shm_handle_.IsValid());
  DCHECK(shm_handle.OwnershipPassesToIPC());
}

ProtectedBufferHandle::ProtectedBufferHandle(
    base::OnceClosure destruction_cb,
    const gfx::NativePixmapHandle& native_pixmap_handle)
    : native_pixmap_handle_(native_pixmap_handle),
      destruction_cb_(std::move(destruction_cb)) {
  DCHECK(!native_pixmap_handle_.fds.empty());
  for (const auto& fd : native_pixmap_handle_.fds)
    DCHECK(fd.auto_close);
}

ProtectedBufferHandle::~ProtectedBufferHandle() {
  if (shm_handle_.OwnershipPassesToIPC())
    shm_handle_.Close();

  for (const auto& fd : native_pixmap_handle_.fds) {
    // Close the fd by wrapping it in a ScopedFD and letting
    // it fall out of scope.
    base::ScopedFD scoped_fd(fd.fd);
  }

  std::move(destruction_cb_).Run();
}

base::SharedMemoryHandle ProtectedBufferHandle::shm_handle() const {
  base::SharedMemoryHandle handle = shm_handle_;
  handle.SetOwnershipPassesToIPC(false);
  return handle;
}

gfx::NativePixmapHandle ProtectedBufferHandle::native_pixmap_handle() const {
  return native_pixmap_handle_;
}

class ProtectedBufferManager::ProtectedBuffer {
 public:
  virtual ~ProtectedBuffer() {}

  // Downcasting methods to return duplicated handles to the underlying
  // protected buffers for each buffer type, or empty/null handles if not
  // applicable.
  virtual base::SharedMemoryHandle DuplicateSharedMemoryHandle() const {
    return base::SharedMemoryHandle();
  }
  virtual gfx::NativePixmapHandle DuplicateNativePixmapHandle() const {
    return gfx::NativePixmapHandle();
  }

  // Downcasting method to return a scoped_refptr to the underlying
  // NativePixmap, or null if not applicable.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap() const {
    return nullptr;
  }

 protected:
  explicit ProtectedBuffer(scoped_refptr<gfx::NativePixmap> dummy_handle)
      : dummy_handle_(std::move(dummy_handle)) {}

 private:
  scoped_refptr<gfx::NativePixmap> dummy_handle_;

  DISALLOW_COPY_AND_ASSIGN(ProtectedBuffer);
};

class ProtectedBufferManager::ProtectedSharedMemory
    : public ProtectedBufferManager::ProtectedBuffer {
 public:
  ~ProtectedSharedMemory() override;

  // Allocate a ProtectedSharedMemory buffer of |size| bytes.
  static std::unique_ptr<ProtectedSharedMemory> Create(
      scoped_refptr<gfx::NativePixmap> dummy_handle,
      size_t size);

  base::SharedMemoryHandle DuplicateSharedMemoryHandle() const override {
    return base::SharedMemory::DuplicateHandle(shmem_->handle());
  }

 private:
  explicit ProtectedSharedMemory(scoped_refptr<gfx::NativePixmap> dummy_handle);

  std::unique_ptr<base::SharedMemory> shmem_;
};

ProtectedBufferManager::ProtectedSharedMemory::ProtectedSharedMemory(
    scoped_refptr<gfx::NativePixmap> dummy_handle)
    : ProtectedBuffer(std::move(dummy_handle)) {}

ProtectedBufferManager::ProtectedSharedMemory::~ProtectedSharedMemory() {}

// static
std::unique_ptr<ProtectedBufferManager::ProtectedSharedMemory>
ProtectedBufferManager::ProtectedSharedMemory::Create(
    scoped_refptr<gfx::NativePixmap> dummy_handle,
    size_t size) {
  std::unique_ptr<ProtectedSharedMemory> protected_shmem(
      new ProtectedSharedMemory(std::move(dummy_handle)));

  size_t aligned_size =
      base::bits::Align(size, base::SysInfo::VMAllocationGranularity());

  mojo::ScopedSharedBufferHandle mojo_shared_buffer =
      mojo::SharedBufferHandle::Create(aligned_size);
  if (!mojo_shared_buffer->is_valid()) {
    VLOGF(1) << "Failed to allocate shared memory";
    return nullptr;
  }

  base::SharedMemoryHandle shm_handle;
  MojoResult mojo_result = mojo::UnwrapSharedMemoryHandle(
      std::move(mojo_shared_buffer), &shm_handle, nullptr, nullptr);
  if (mojo_result != MOJO_RESULT_OK) {
    VLOGF(1) << "Failed to unwrap a mojo shared memory handle";
    return nullptr;
  }

  protected_shmem->shmem_ =
      std::make_unique<base::SharedMemory>(shm_handle, false);
  return protected_shmem;
}

class ProtectedBufferManager::ProtectedNativePixmap
    : public ProtectedBufferManager::ProtectedBuffer {
 public:
  ~ProtectedNativePixmap() override;

  // Allocate a ProtectedNativePixmap of |format| and |size|.
  static std::unique_ptr<ProtectedNativePixmap> Create(
      scoped_refptr<gfx::NativePixmap> dummy_handle,
      gfx::BufferFormat format,
      const gfx::Size& size);

  gfx::NativePixmapHandle DuplicateNativePixmapHandle() const override {
    return native_pixmap_->ExportHandle();
  }

  scoped_refptr<gfx::NativePixmap> GetNativePixmap() const override {
    return native_pixmap_;
  }

 private:
  explicit ProtectedNativePixmap(scoped_refptr<gfx::NativePixmap> dummy_handle);

  scoped_refptr<gfx::NativePixmap> native_pixmap_;
};

ProtectedBufferManager::ProtectedNativePixmap::ProtectedNativePixmap(
    scoped_refptr<gfx::NativePixmap> dummy_handle)
    : ProtectedBuffer(std::move(dummy_handle)) {}

ProtectedBufferManager::ProtectedNativePixmap::~ProtectedNativePixmap() {}

// static
std::unique_ptr<ProtectedBufferManager::ProtectedNativePixmap>
ProtectedBufferManager::ProtectedNativePixmap::Create(
    scoped_refptr<gfx::NativePixmap> dummy_handle,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  std::unique_ptr<ProtectedNativePixmap> protected_pixmap(
      new ProtectedNativePixmap(std::move(dummy_handle)));

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  protected_pixmap->native_pixmap_ =
      factory->CreateNativePixmap(gfx::kNullAcceleratedWidget, size, format,
                                  gfx::BufferUsage::SCANOUT_VDA_WRITE);

  if (!protected_pixmap->native_pixmap_) {
    VLOGF(1) << "Failed allocating a native pixmap";
    return nullptr;
  }

  return protected_pixmap;
}

ProtectedBufferManager::ProtectedBufferManager() : weak_factory_(this) {
  VLOGF(2);
  weak_this_ = weak_factory_.GetWeakPtr();
}

ProtectedBufferManager::~ProtectedBufferManager() {
  VLOGF(2);
}

std::unique_ptr<ProtectedBufferHandle>
ProtectedBufferManager::AllocateProtectedSharedMemory(base::ScopedFD dummy_fd,
                                                      size_t size) {
  VLOGF(2) << "dummy_fd: " << dummy_fd.get() << ", size: " << size;

  // Import the |dummy_fd| to produce a unique id for it.
  uint32_t id;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return nullptr;

  base::AutoLock lock(buffer_map_lock_);

  if (buffer_map_.find(id) != buffer_map_.end()) {
    VLOGF(1) << "A protected buffer for this handle already exists";
    return nullptr;
  }

  // Allocate a protected buffer and associate it with the dummy pixmap.
  // The pixmap needs to be stored to ensure the id remains the same for
  // the entire lifetime of the dummy pixmap.
  auto protected_shmem = ProtectedSharedMemory::Create(pixmap, size);
  if (!protected_shmem) {
    VLOGF(1) << "Failed allocating a protected shared memory buffer";
    return nullptr;
  }

  auto shm_handle = protected_shmem->DuplicateSharedMemoryHandle();
  if (!shm_handle.IsValid()) {
    VLOGF(1) << "Failed duplicating SharedMemoryHandle";
    return nullptr;
  }

  // Store the buffer in the buffer_map_, and return a handle to it to the
  // client. The buffer will be permanently removed from the map when the
  // handle is destroyed.
  VLOGF(2) << "New protected shared memory buffer, handle id: " << id;
  auto protected_buffer_handle = std::make_unique<ProtectedBufferHandle>(
      base::BindOnce(&ProtectedBufferManager::RemoveEntry, weak_this_, id),
      shm_handle);

  // This will always succeed as we find() first above.
  buffer_map_.emplace(id, std::move(protected_shmem));

  return protected_buffer_handle;
}

std::unique_ptr<ProtectedBufferHandle>
ProtectedBufferManager::AllocateProtectedNativePixmap(base::ScopedFD dummy_fd,
                                                      gfx::BufferFormat format,
                                                      const gfx::Size& size) {
  VLOGF(2) << "dummy_fd: " << dummy_fd.get() << ", format: " << (int)format
           << ", size: " << size.ToString();

  // Import the |dummy_fd| to produce a unique id for it.
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return nullptr;

  base::AutoLock lock(buffer_map_lock_);

  if (buffer_map_.find(id) != buffer_map_.end()) {
    VLOGF(1) << "A protected buffer for this handle already exists";
    return nullptr;
  }

  // Allocate a protected buffer and associate it with the dummy pixmap.
  // The pixmap needs to be stored to ensure the id remains the same for
  // the entire lifetime of the dummy pixmap.
  auto protected_pixmap = ProtectedNativePixmap::Create(pixmap, format, size);
  if (!protected_pixmap) {
    VLOGF(1) << "Failed allocating a protected native pixmap";
    return nullptr;
  }

  auto native_pixmap_handle = protected_pixmap->DuplicateNativePixmapHandle();
  if (native_pixmap_handle.planes.empty()) {
    VLOGF(1) << "Failed duplicating NativePixmapHandle";
    return nullptr;
  }

  // Store the buffer in the buffer_map_, and return a handle to it to the
  // client. The buffer will be permanently removed from the map when the
  // handle is destroyed.
  VLOGF(2) << "New protected native pixmap, handle id: " << id;
  auto protected_buffer_handle = std::make_unique<ProtectedBufferHandle>(
      base::BindOnce(&ProtectedBufferManager::RemoveEntry, weak_this_, id),
      native_pixmap_handle);

  // This will always succeed as we find() first above.
  buffer_map_.emplace(id, std::move(protected_pixmap));

  return protected_buffer_handle;
}

base::SharedMemoryHandle
ProtectedBufferManager::GetProtectedSharedMemoryHandleFor(
    base::ScopedFD dummy_fd) {
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return base::SharedMemoryHandle();

  return iter->second->DuplicateSharedMemoryHandle();
}

gfx::NativePixmapHandle
ProtectedBufferManager::GetProtectedNativePixmapHandleFor(
    base::ScopedFD dummy_fd) {
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return gfx::NativePixmapHandle();

  return iter->second->DuplicateNativePixmapHandle();
}

scoped_refptr<gfx::NativePixmap>
ProtectedBufferManager::GetProtectedNativePixmapFor(
    const gfx::NativePixmapHandle& handle) {
  // Only the first fd is used for lookup.
  if (handle.fds.empty())
    return nullptr;

  base::ScopedFD dummy_fd(HANDLE_EINTR(dup(handle.fds[0].fd)));
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return nullptr;

  auto native_pixmap = iter->second->GetNativePixmap();
  if (native_pixmap) {
    for (const auto& fd : handle.fds)
      base::ScopedFD scoped_fd(fd.fd);
  }

  return native_pixmap;
}

scoped_refptr<gfx::NativePixmap> ProtectedBufferManager::ImportDummyFd(
    base::ScopedFD dummy_fd,
    uint32_t* id) const {
  // 0 is an invalid handle id.
  *id = 0;

  // Import dummy_fd to acquire its unique id.
  // CreateNativePixmapFromHandle() takes ownership and will close the handle
  // also on failure.
  gfx::NativePixmapHandle pixmap_handle;
  pixmap_handle.fds.emplace_back(
      base::FileDescriptor(dummy_fd.release(), true));
  pixmap_handle.planes.emplace_back(gfx::NativePixmapPlane());
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  scoped_refptr<gfx::NativePixmap> pixmap =
      factory->CreateNativePixmapForProtectedBufferHandle(
          gfx::kNullAcceleratedWidget, kDummyBufferSize, gfx::BufferFormat::R_8,
          pixmap_handle);
  if (!pixmap) {
    VLOGF(1) << "Failed importing dummy handle";
    return nullptr;
  }

  *id = pixmap->GetUniqueId();
  if (*id == 0) {
    VLOGF(1) << "Failed acquiring unique id for handle";
    return nullptr;
  }

  return pixmap;
}

void ProtectedBufferManager::RemoveEntry(uint32_t id) {
  VLOGF(2) << "id: " << id;

  base::AutoLock lock(buffer_map_lock_);
  auto num_erased = buffer_map_.erase(id);
  if (num_erased != 1)
    VLOGF(1) << "No buffer id " << id << " to destroy";
}

}  // namespace arc
