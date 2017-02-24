// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/display.h"

#include <iterator>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/notification_surface.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/shared_memory.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

#if defined(USE_OZONE)
#include <GLES2/gl2extchromium.h>
#include "components/exo/buffer.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl_ozone_native_pixmap.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace exo {
namespace {
#if defined(USE_OZONE)
// TODO(dcastagna): The following formats should be determined at runtime
// querying kms (via ozone).
const gfx::BufferFormat kOverlayFormats[] = {
// TODO(dcastagna): Remove RGBX/RGBA once all the platforms using the fullscreen
// optimization will have switched to atomic.
#if defined(ARCH_CPU_ARM_FAMILY)
    gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::RGBA_8888,
#endif
    gfx::BufferFormat::BGRX_8888, gfx::BufferFormat::BGRA_8888};

const gfx::BufferFormat kOverlayFormatsForDrmAtomic[] = {
    gfx::BufferFormat::RGBX_8888, gfx::BufferFormat::RGBA_8888,
    gfx::BufferFormat::BGR_565};
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Display, public:

Display::Display() : Display(nullptr) {}

Display::Display(NotificationSurfaceManager* notification_surface_manager)
    : notification_surface_manager_(notification_surface_manager)
#if defined(USE_OZONE)
      ,
      overlay_formats_(std::begin(kOverlayFormats), std::end(kOverlayFormats))
#endif
{
#if defined(USE_OZONE)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDrmAtomic)) {
    overlay_formats_.insert(overlay_formats_.end(),
                            std::begin(kOverlayFormatsForDrmAtomic),
                            std::end(kOverlayFormatsForDrmAtomic));
  }
#endif
}

Display::~Display() {}

std::unique_ptr<Surface> Display::CreateSurface() {
  TRACE_EVENT0("exo", "Display::CreateSurface");

  return base::WrapUnique(new Surface);
}

std::unique_ptr<SharedMemory> Display::CreateSharedMemory(
    const base::SharedMemoryHandle& handle,
    size_t size) {
  TRACE_EVENT1("exo", "Display::CreateSharedMemory", "size", size);

  if (!base::SharedMemory::IsHandleValid(handle))
    return nullptr;

  return base::MakeUnique<SharedMemory>(handle);
}

#if defined(USE_OZONE)
std::unique_ptr<Buffer> Display::CreateLinuxDMABufBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    const std::vector<gfx::NativePixmapPlane>& planes,
    std::vector<base::ScopedFD>&& fds) {
  TRACE_EVENT1("exo", "Display::CreateLinuxDMABufBuffer", "size",
               size.ToString());

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::OZONE_NATIVE_PIXMAP;
  for (auto& fd : fds)
    handle.native_pixmap_handle.fds.emplace_back(std::move(fd));

  for (auto& plane : planes)
    handle.native_pixmap_handle.planes.push_back(plane);

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu::GpuMemoryBufferImplOzoneNativePixmap::CreateFromHandle(
          handle, size, format, gfx::BufferUsage::GPU_READ,
          gpu::GpuMemoryBufferImpl::DestructionCallback());
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer from handle";
    return nullptr;
  }

  // Using zero-copy for optimal performance.
  bool use_zero_copy = true;

  bool is_overlay_candidate =
      std::find(overlay_formats_.begin(), overlay_formats_.end(), format) !=
      overlay_formats_.end();

  return base::MakeUnique<Buffer>(
      std::move(gpu_memory_buffer), GL_TEXTURE_EXTERNAL_OES,
      // COMMANDS_COMPLETED queries are required by native pixmaps.
      GL_COMMANDS_COMPLETED_CHROMIUM, use_zero_copy, is_overlay_candidate);
}
#endif

std::unique_ptr<ShellSurface> Display::CreateShellSurface(Surface* surface) {
  TRACE_EVENT1("exo", "Display::CreateShellSurface", "surface",
               surface->AsTracedValue());

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return base::MakeUnique<ShellSurface>(
      surface, nullptr, ShellSurface::BoundsMode::SHELL, gfx::Point(),
      true /* activatable */, false /* can_minimize */,
      ash::kShellWindowId_DefaultContainer);
}

std::unique_ptr<ShellSurface> Display::CreatePopupShellSurface(
    Surface* surface,
    ShellSurface* parent,
    const gfx::Point& position) {
  TRACE_EVENT2("exo", "Display::CreatePopupShellSurface", "surface",
               surface->AsTracedValue(), "parent", parent->AsTracedValue());

  if (surface->window()->Contains(parent->GetWidget()->GetNativeWindow())) {
    DLOG(ERROR) << "Parent is contained within surface's hierarchy";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  // |position| is relative to the parent's main surface origin, and |origin| is
  // in screen coordinates.
  gfx::Point origin = position;
  wm::ConvertPointToScreen(
      ShellSurface::GetMainSurface(parent->GetWidget()->GetNativeWindow())
          ->window(),
      &origin);

  return base::MakeUnique<ShellSurface>(
      surface, parent, ShellSurface::BoundsMode::FIXED, origin,
      false /* activatable */, false /* can_minimize */,
      ash::kShellWindowId_DefaultContainer);
}

std::unique_ptr<ShellSurface> Display::CreateRemoteShellSurface(
    Surface* surface,
    const gfx::Point& origin,
    int container) {
  TRACE_EVENT2("exo", "Display::CreateRemoteShellSurface", "surface",
               surface->AsTracedValue(), "container", container);

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  // Remote shell surfaces in system modal container cannot be minimized.
  bool can_minimize = container != ash::kShellWindowId_SystemModalContainer;

  return base::MakeUnique<ShellSurface>(
      surface, nullptr, ShellSurface::BoundsMode::CLIENT, origin,
      true /* activatable */, can_minimize, container);
}

std::unique_ptr<SubSurface> Display::CreateSubSurface(Surface* surface,
                                                      Surface* parent) {
  TRACE_EVENT2("exo", "Display::CreateSubSurface", "surface",
               surface->AsTracedValue(), "parent", parent->AsTracedValue());

  if (surface->window()->Contains(parent->window())) {
    DLOG(ERROR) << "Parent is contained within surface's hierarchy";
    return nullptr;
  }

  if (surface->HasSurfaceDelegate()) {
    DLOG(ERROR) << "Surface has already been assigned a role";
    return nullptr;
  }

  return base::MakeUnique<SubSurface>(surface, parent);
}

std::unique_ptr<NotificationSurface> Display::CreateNotificationSurface(
    Surface* surface,
    const std::string& notification_id) {
  TRACE_EVENT2("exo", "Display::CreateNotificationSurface", "surface",
               surface->AsTracedValue(), "notification_id", notification_id);

  if (!notification_surface_manager_ ||
      notification_surface_manager_->GetSurface(notification_id)) {
    DLOG(ERROR) << "Invalid notification id, id=" << notification_id;
    return nullptr;
  }

  return base::MakeUnique<NotificationSurface>(notification_surface_manager_,
                                               surface, notification_id);
}

}  // namespace exo
