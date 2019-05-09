// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include <dxgi1_6.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/service/dc_layer_tree.h"
#include "gpu/ipc/service/direct_composition_child_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/vsync_thread_win.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

namespace gpu {
namespace {
struct OverlaySupportInfo {
  OverlayFormat overlay_format;
  DXGI_FORMAT dxgi_format;
  UINT flags;
};

// Indicates support for either NV12 or YUY2 hardware overlays.
bool g_supports_overlays = false;

// Indicates support for hardware overlay scaling.
bool g_supports_scaled_overlays = true;

// Used for workaround limiting overlay size to monitor size.
gfx::Size g_overlay_monitor_size;

// Preferred overlay format set when detecting hardware overlay support during
// initialization.  Set to NV12 by default so that it's used when enabling
// overlays using command line flags.
OverlayFormat g_overlay_format_used = OverlayFormat::kNV12;
DXGI_FORMAT g_overlay_dxgi_format_used = DXGI_FORMAT_NV12;

// This is the raw support info, which shouldn't depend on field trial state, or
// command line flags. Ordered by most preferred to least preferred format.
OverlaySupportInfo g_overlay_support_info[] = {
    {OverlayFormat::kNV12, DXGI_FORMAT_NV12, 0},
    {OverlayFormat::kYUY2, DXGI_FORMAT_YUY2, 0},
    {OverlayFormat::kBGRA, DXGI_FORMAT_B8G8R8A8_UNORM, 0},
};

void InitializeHardwareOverlaySupport() {
  static bool overlay_support_initialized = false;
  if (overlay_support_initialized)
    return;
  overlay_support_initialized = true;

  // Check for DirectComposition support first to prevent likely crashes.
  if (!DirectCompositionSurfaceWin::IsDirectCompositionSupported())
    return;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes wouldn't
  // be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS1)
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.As(&dxgi_device))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return;
  }

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.As(&video_device))) {
    DLOG(ERROR) << "Failed to retrieve video device";
    return;
  }

  bool supports_nv12_rec709 = false;
  unsigned int i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, &output)))
      break;
    DCHECK(output);
    Microsoft::WRL::ComPtr<IDXGIOutput3> output3;
    if (FAILED(output.As(&output3)))
      continue;
    DCHECK(output3);

    for (auto& info : g_overlay_support_info) {
      if (FAILED(output3->CheckOverlaySupport(
              info.dxgi_format, d3d11_device.Get(), &info.flags))) {
        continue;
      }
      // Per Intel's request, use NV12 only when
      // COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is also supported. Rec 709 is
      // commonly used for H.264 and HEVC. At least one Intel Gen9 SKU will not
      // support NV12 overlays.
      if (info.overlay_format == OverlayFormat::kNV12) {
        UINT color_space_support_flags = 0;
        Microsoft::WRL::ComPtr<IDXGIOutput4> output4;
        if (FAILED(output.As(&output4)))
          continue;

        if (FAILED(output4->CheckOverlayColorSpaceSupport(
                info.dxgi_format, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
                d3d11_device.Get(), &color_space_support_flags))) {
          continue;
        }
        supports_nv12_rec709 =
            !!(color_space_support_flags &
               DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
      }

      // Formats are ordered by most preferred to least preferred. Don't choose
      // a less preferred format, but keep going so that we can record overlay
      // support for all formats in UMA.
      if (g_supports_overlays)
        continue;
      // Don't use BGRA overlays in any case, but record support in UMA.
      if (info.overlay_format == OverlayFormat::kBGRA)
        continue;
      // Overlays are supported for NV12 only if the feature flag to prefer NV12
      // over YUY2 is enabled.
      bool prefer_nv12 = base::FeatureList::IsEnabled(
          features::kDirectCompositionPreferNV12Overlays);
      if (info.overlay_format == OverlayFormat::kNV12 &&
          (!prefer_nv12 || !supports_nv12_rec709))
        continue;
      // Some new Intel drivers only claim to support unscaled overlays, but
      // scaled overlays still work. It's possible DWM works around it by
      // performing an extra scaling Blt before calling the driver. Even when
      // scaled overlays aren't actually supported, presentation using the
      // overlay path should be relatively efficient.
      if (info.flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                        DXGI_OVERLAY_SUPPORT_FLAG_SCALING)) {
        g_overlay_format_used = info.overlay_format;
        g_overlay_dxgi_format_used = info.dxgi_format;

        g_supports_overlays = true;
        g_supports_scaled_overlays =
            !!(info.flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);

        DXGI_OUTPUT_DESC monitor_desc = {};
        if (SUCCEEDED(output3->GetDesc(&monitor_desc))) {
          g_overlay_monitor_size =
              gfx::Rect(monitor_desc.DesktopCoordinates).size();
        }
      }
    }
    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    // TODO(sunnyps): If the above is true, then we can only look at first
    // output instead of iterating over all outputs.
    if (g_supports_overlays)
      break;
  }
  if (g_supports_overlays) {
    UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.OverlayFormatUsed2",
                              g_overlay_format_used);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlaysSupported",
                        g_supports_overlays);
}
}  // namespace

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    std::unique_ptr<gfx::VSyncProvider> vsync_provider,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::GLSurfaceEGL(),
      child_window_(delegate, parent_window),
      root_surface_(new DirectCompositionChildSurfaceWin()),
      layer_tree_(std::make_unique<DCLayerTree>(
          delegate->GetFeatureInfo()->workarounds())),
      delegate_(delegate),
      vsync_provider_(std::move(vsync_provider)),
      presentation_helper_(std::make_unique<gl::GLSurfacePresentationHelper>(
          vsync_provider_.get())) {}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

// static
bool DirectCompositionSurfaceWin::IsDirectCompositionSupported() {
  static const bool supported = [] {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDisableDirectComposition))
      return false;

    // Blacklist direct composition if MCTU.dll or MCTUX.dll are injected. These
    // are user mode drivers for display adapters from Magic Control Technology
    // Corporation.
    if (GetModuleHandle(TEXT("MCTU.dll")) ||
        GetModuleHandle(TEXT("MCTUX.dll"))) {
      DLOG(ERROR) << "Blacklisted due to third party modules";
      return false;
    }

    // Flexible surface compatibility is required to be able to MakeCurrent with
    // the default pbuffer surface.
    if (!gl::GLSurfaceEGL::IsEGLFlexibleSurfaceCompatibilitySupported()) {
      DLOG(ERROR) << "EGL_ANGLE_flexible_surface_compatibility not supported";
      return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
        gl::QueryD3D11DeviceObjectFromANGLE();
    if (!d3d11_device) {
      DLOG(ERROR) << "Failed to retrieve D3D11 device";
      return false;
    }

    // This will fail if the D3D device is "Microsoft Basic Display Adapter".
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
    if (FAILED(d3d11_device.As(&video_device))) {
      DLOG(ERROR) << "Failed to retrieve video device";
      return false;
    }

    // This will fail if DirectComposition DLL can't be loaded.
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
        gl::QueryDirectCompositionDevice(d3d11_device);
    if (!dcomp_device) {
      DLOG(ERROR) << "Failed to retrieve direct composition device";
      return false;
    }

    return true;
  }();
  return supported;
}

// static
bool DirectCompositionSurfaceWin::AreOverlaysSupported() {
  // Always initialize and record overlay support information irrespective of
  // command line flags.
  InitializeHardwareOverlaySupport();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Enable flag should be checked before the disable flag, so we could
  // overwrite GPU driver bug workarounds in testing.
  if (command_line->HasSwitch(switches::kEnableDirectCompositionLayers))
    return true;
  if (command_line->HasSwitch(switches::kDisableDirectCompositionLayers))
    return false;

  return g_supports_overlays;
}

// static
void DirectCompositionSurfaceWin::DisableOverlays() {
  g_supports_overlays = false;
}

// static
bool DirectCompositionSurfaceWin::AreScaledOverlaysSupported() {
  return g_supports_scaled_overlays;
}

// static
OverlayCapabilities DirectCompositionSurfaceWin::GetOverlayCapabilities() {
  InitializeHardwareOverlaySupport();
  OverlayCapabilities capabilities;
  for (const auto& info : g_overlay_support_info) {
    if (info.flags) {
      OverlayCapability cap;
      cap.format = info.overlay_format;
      cap.is_scaling_supported =
          !!(info.flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
      capabilities.push_back(cap);
    }
  }
  return capabilities;
}

// static
gfx::Size DirectCompositionSurfaceWin::GetOverlayMonitorSize() {
  return g_overlay_monitor_size;
}

// static
OverlayFormat DirectCompositionSurfaceWin::GetOverlayFormatUsed() {
  return g_overlay_format_used;
}

// static
DXGI_FORMAT DirectCompositionSurfaceWin::GetOverlayDxgiFormatUsed() {
  return g_overlay_dxgi_format_used;
}

// static
void DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(
    bool value) {
  g_supports_scaled_overlays = value;
}

// static
void DirectCompositionSurfaceWin::SetPreferNV12OverlaysForTesting() {
  g_overlay_format_used = OverlayFormat::kNV12;
  g_overlay_dxgi_format_used = DXGI_FORMAT_NV12;
}

// static
bool DirectCompositionSurfaceWin::IsHDRSupported() {
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::Version::WIN10_RS2)
    return false;

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory> factory;
  hr = CreateDXGIFactory(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DXGI factory.";
    return false;
  }

  bool hdr_monitor_found = false;
  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, &output);
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(IID_PPV_ARGS(&output6));
      if (FAILED(hr)) {
        DLOG(WARNING) << "IDXGIOutput6 is required for HDR detection.";
        continue;
      }

      DXGI_OUTPUT_DESC1 desc;
      if (FAILED(output6->GetDesc1(&desc))) {
        DLOG(ERROR) << "Unexpected error getting output descriptor.";
        continue;
      }

      base::UmaHistogramSparse("GPU.Output.ColorSpace", desc.ColorSpace);
      base::UmaHistogramSparse("GPU.Output.MaxLuminance", desc.MaxLuminance);

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        hdr_monitor_found = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.Output.HDR", hdr_monitor_found);
  return hdr_monitor_found;
}

bool DirectCompositionSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device_) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device from ANGLE";
    return false;
  }

  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_) {
    DLOG(ERROR)
        << "Failed to retrieve direct compostion device from D3D11 device";
    return false;
  }

  if (!child_window_.Initialize()) {
    DLOG(ERROR) << "Failed to initialize native window";
    return false;
  }
  window_ = child_window_.window();

  auto vsync_callback = delegate_->GetGpuVSyncCallback();
  if (SupportsGpuVSync() && vsync_callback) {
    vsync_thread_ = std::make_unique<gl::VSyncThreadWin>(
        window_, d3d11_device_, std::move(vsync_callback));
  }

  if (!layer_tree_->Initialize(window_, d3d11_device_, dcomp_device_))
    return false;

  if (!root_surface_->Initialize(gl::GLSurfaceFormat()))
    return false;

  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  // Destroy vsync thread because joining it could call OnVSync.
  vsync_thread_ = nullptr;
  // Destroy presentation helper first because its dtor calls GetHandle.
  presentation_helper_ = nullptr;
  root_surface_->Destroy();
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return root_surface_->GetSize();
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return root_surface_->GetHandle();
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         ColorSpace color_space,
                                         bool has_alpha) {
  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  return root_surface_->Resize(size, scale_factor, color_space, has_alpha);
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT0("gpu", "DirectCompositionSurfaceWin::SwapBuffers");
  gl::GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));

  gfx::SwapResult swap_result =
      root_surface_->SwapBuffers(PresentationCallback());

  if (!layer_tree_->CommitAndClearPendingOverlays(root_surface_.get()))
    swap_result = gfx::SwapResult::SWAP_FAILED;

  scoped_swap_buffers.set_result(swap_result);
  return swap_result;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(std::move(callback));
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return vsync_provider_.get();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  root_surface_->SetVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    const ui::DCRendererLayerParams& params) {
  return layer_tree_->ScheduleDCLayer(params);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  return root_surface_->SetEnableDCLayers(enable);
}

bool DirectCompositionSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPresentationCallback() {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  return root_surface_->OnMakeCurrent(context);
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::UseOverlaysForVideo() const {
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SupportsProtectedVideo() const {
  // TODO(magchen): Check the gpu driver date (or a function) which we know this
  // new support is enabled.
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  return root_surface_->SetDrawRectangle(rectangle);
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  return root_surface_->GetDrawOffset();
}

bool DirectCompositionSurfaceWin::SupportsGpuVSync() const {
  return base::FeatureList::IsEnabled(features::kDirectCompositionGpuVSync);
}

void DirectCompositionSurfaceWin::SetGpuVSyncEnabled(bool enabled) {
  DCHECK(vsync_thread_);
  vsync_thread_->SetEnabled(enabled);
}

scoped_refptr<base::TaskRunner>
DirectCompositionSurfaceWin::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetBackbufferSwapChainForTesting() const {
  return root_surface_->swap_chain();
}

}  // namespace gpu
