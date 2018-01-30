// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include <d3d11_1.h>
#include <dcomptypes.h>
#include <dxgi1_6.h>

#include "base/containers/circular_deque.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/ipc/service/direct_composition_child_surface_win.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/transform.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/scoped_make_current.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

namespace gpu {
namespace {

// Some drivers fail to correctly handle BT.709 video in overlays. This flag
// converts them to BT.601 in the video processor.
const base::Feature kFallbackBT709VideoToBT601{
    "FallbackBT709VideoToBT601", base::FEATURE_DISABLED_BY_DEFAULT};

bool SizeContains(const gfx::Size& a, const gfx::Size& b) {
  return gfx::Rect(a).Contains(gfx::Rect(b));
}

// This keeps track of whether the previous 30 frames used Overlays or GPU
// composition to present.
class PresentationHistory {
 public:
  static const int kPresentsToStore = 30;

  PresentationHistory() {}

  void AddSample(DXGI_FRAME_PRESENTATION_MODE mode) {
    if (mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
      composed_count_++;

    presents_.push_back(mode);
    if (presents_.size() > kPresentsToStore) {
      DXGI_FRAME_PRESENTATION_MODE first_mode = presents_.front();
      if (first_mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
        composed_count_--;
      presents_.pop_front();
    }
  }

  bool valid() const { return presents_.size() >= kPresentsToStore; }
  int composed_count() const { return composed_count_; }

 private:
  base::circular_deque<DXGI_FRAME_PRESENTATION_MODE> presents_;
  int composed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PresentationHistory);
};

gfx::Size g_overlay_monitor_size;

bool g_supports_scaled_overlays = true;

// This is the raw support info, which shouldn't depend on field trial state.
bool HardwareSupportsOverlays() {
  if (!gl::GLSurfaceEGL::IsDirectCompositionSupported())
    return false;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableDirectCompositionLayers))
    return true;
  if (command_line->HasSwitch(switches::kDisableDirectCompositionLayers))
    return false;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes
  // wouldn't be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return false;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failing to create overlay swapchain because couldn't "
                   "retrieve D3D11 device from ANGLE.";
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.CopyTo(video_device.GetAddressOf()))) {
    DLOG(ERROR) << "Failing to create overlay swapchain because couldn't "
                   "retrieve video device from D3D11 device.";
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.CopyTo(dxgi_device.GetAddressOf());
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());

  unsigned int i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, output.GetAddressOf())))
      break;
    Microsoft::WRL::ComPtr<IDXGIOutput3> output3;
    if (FAILED(output.CopyTo(output3.GetAddressOf())))
      continue;

    UINT flags = 0;
    if (FAILED(output3->CheckOverlaySupport(DXGI_FORMAT_YUY2,
                                            d3d11_device.Get(), &flags)))
      continue;

    base::UmaHistogramSparse("GPU.DirectComposition.OverlaySupportFlags",
                             flags);

    // Some new Intel drivers only claim to support unscaled overlays, but
    // scaled overlays still work. Even when scaled overlays aren't actually
    // supported, presentation using the overlay path should be relatively
    // efficient.
    if (flags & (DXGI_OVERLAY_SUPPORT_FLAG_SCALING |
                 DXGI_OVERLAY_SUPPORT_FLAG_DIRECT)) {
      DXGI_OUTPUT_DESC monitor_desc = {};
      if (FAILED(output3->GetDesc(&monitor_desc)))
        continue;
      g_overlay_monitor_size =
          gfx::Rect(monitor_desc.DesktopCoordinates).size();
      g_supports_scaled_overlays =
          !!(flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
      return true;
    }
  }
  return false;
}

}  // namespace

class DCLayerTree {
 public:
  DCLayerTree(DirectCompositionSurfaceWin* surface,
              const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
              const Microsoft::WRL::ComPtr<IDCompositionDevice2>& dcomp_device)
      : surface_(surface),
        d3d11_device_(d3d11_device),
        dcomp_device_(dcomp_device) {}

  bool Initialize(HWND window);
  bool CommitAndClearPendingOverlays();
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params);
  void InitializeVideoProcessor(const gfx::Size& input_size,
                                const gfx::Size& output_size);

  const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor() const {
    return video_processor_;
  }
  const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>&
  video_processor_enumerator() const {
    return video_processor_enumerator_;
  }
  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  const GpuDriverBugWorkarounds& workarounds() const {
    return surface_->workarounds();
  }

 private:
  class SwapChainPresenter;

  // This struct is used to cache information about what visuals are currently
  // being presented so that properties that aren't changed aren't sent to
  // DirectComposition.
  struct VisualInfo {
    Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> clip_visual;

    std::unique_ptr<SwapChainPresenter> swap_chain_presenter;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    Microsoft::WRL::ComPtr<IDCompositionSurface> surface;
    uint64_t dcomp_surface_serial = 0;

    gfx::Rect bounds;
    float swap_chain_scale_x = 0.0f;
    float swap_chain_scale_y = 0.0f;
    bool is_clipped = false;
    gfx::Rect clip_rect;
    gfx::Transform transform;
  };

  // These functions return true if the visual tree was changed.
  bool InitVisual(size_t i);
  bool UpdateVisualForVideo(VisualInfo* visual_info,
                            const ui::DCRendererLayerParams& params);
  bool UpdateVisualForBackbuffer(VisualInfo* visual_info,
                                 const ui::DCRendererLayerParams& params);
  bool UpdateVisualClip(VisualInfo* visual_info,
                        const ui::DCRendererLayerParams& params);

  DirectCompositionSurfaceWin* surface_;
  std::vector<std::unique_ptr<ui::DCRendererLayerParams>> pending_overlays_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;
  Microsoft::WRL::ComPtr<IDCompositionVisual2> root_visual_;

  // The video processor is cached so SwapChains don't have to recreate it
  // whenever they're created.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;
  gfx::Size video_input_size_;
  gfx::Size video_output_size_;

  std::vector<VisualInfo> visual_info_;

  DISALLOW_COPY_AND_ASSIGN(DCLayerTree);
};

class DCLayerTree::SwapChainPresenter {
 public:
  SwapChainPresenter(DCLayerTree* surface,
                     Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

  ~SwapChainPresenter();

  void PresentToSwapChain(const ui::DCRendererLayerParams& overlay);

  float swap_chain_scale_x() const { return swap_chain_scale_x_; }
  float swap_chain_scale_y() const { return swap_chain_scale_y_; }
  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

 private:
  using PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE =
      HRESULT(WINAPI*)(DWORD, SECURITY_ATTRIBUTES*, HANDLE*);

  bool UploadVideoImages(gl::GLImageMemory* y_image_memory,
                         gl::GLImageMemory* uv_image_memory);
  // Returns true if the video processor changed.
  bool InitializeVideoProcessor(const gfx::Size& in_size,
                                const gfx::Size& out_size);
  void ReallocateSwapChain(bool yuy2);
  bool ShouldBeYUY2();

  DCLayerTree* surface_;
  PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE create_surface_handle_function_;

  gfx::Size swap_chain_size_;
  gfx::Size processor_input_size_;
  gfx::Size processor_output_size_;
  bool is_yuy2_swapchain_ = false;

  // This is the scale from the swapchain size to the size of the contents
  // onscreen.
  float swap_chain_scale_x_ = 0.0f;
  float swap_chain_scale_y_ = 0.0f;

  PresentationHistory presentation_history_;
  bool failed_to_create_yuy2_swapchain_ = false;
  int frames_since_color_space_change_ = 0;

  // These are the GLImages that were presented in the last frame.
  std::vector<scoped_refptr<gl::GLImage>> last_gl_images_;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  gfx::Size staging_texture_size_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> out_view_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;

  base::win::ScopedHandle swap_chain_handle_;

  DISALLOW_COPY_AND_ASSIGN(SwapChainPresenter);
};

bool DCLayerTree::Initialize(HWND window) {
  HRESULT hr = d3d11_device_.CopyTo(video_device_.GetAddressOf());
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  hr = context.CopyTo(video_context_.GetAddressOf());
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.CopyTo(desktop_device.GetAddressOf());

  hr = desktop_device->CreateTargetForHwnd(window, TRUE,
                                           dcomp_target_.GetAddressOf());
  if (FAILED(hr))
    return false;

  hr = dcomp_device_->CreateVisual(root_visual_.GetAddressOf());
  if (FAILED(hr))
    return false;

  dcomp_target_->SetRoot(root_visual_.Get());
  return true;
}

void DCLayerTree::InitializeVideoProcessor(const gfx::Size& input_size,
                                           const gfx::Size& output_size) {
  if (SizeContains(video_input_size_, input_size) &&
      SizeContains(video_output_size_, output_size))
    return;
  video_input_size_ = input_size;
  video_output_size_ = output_size;

  video_processor_.Reset();
  video_processor_enumerator_.Reset();
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = input_size.width();
  desc.InputHeight = input_size.height();
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = output_size.width();
  desc.OutputHeight = output_size.height();
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
  HRESULT hr = video_device_->CreateVideoProcessorEnumerator(
      &desc, video_processor_enumerator_.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  hr = video_device_->CreateVideoProcessor(video_processor_enumerator_.Get(), 0,
                                           video_processor_.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  // Auto stream processing (the default) can hurt power consumption.
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, FALSE);
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DCLayerTree::GetLayerSwapChainForTesting(size_t index) const {
  if (index >= visual_info_.size())
    return Microsoft::WRL::ComPtr<IDXGISwapChain1>();
  return visual_info_[index].swap_chain;
}

DCLayerTree::SwapChainPresenter::SwapChainPresenter(
    DCLayerTree* surface,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device)
    : surface_(surface), d3d11_device_(d3d11_device) {
  HRESULT hr = d3d11_device_.CopyTo(video_device_.GetAddressOf());
  CHECK(SUCCEEDED(hr));
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  hr = context.CopyTo(video_context_.GetAddressOf());
  CHECK(SUCCEEDED(hr));
  HMODULE dcomp = ::GetModuleHandleA("dcomp.dll");
  CHECK(dcomp);
  create_surface_handle_function_ =
      reinterpret_cast<PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE>(
          GetProcAddress(dcomp, "DCompositionCreateSurfaceHandle"));
  CHECK(create_surface_handle_function_);
}

DCLayerTree::SwapChainPresenter::~SwapChainPresenter() {}

bool DCLayerTree::SwapChainPresenter::ShouldBeYUY2() {
  // Start out as YUY2.
  if (!presentation_history_.valid())
    return true;
  int composition_count = presentation_history_.composed_count();

  // It's more efficient to use a BGRA backbuffer instead of YUY2 if overlays
  // aren't being used, as otherwise DWM will use the video processor a second
  // time to convert it to BGRA before displaying it on screen.

  if (is_yuy2_swapchain_) {
    // Switch to BGRA once 3/4 of presents are composed.
    return composition_count < (PresentationHistory::kPresentsToStore * 3 / 4);
  } else {
    // Switch to YUY2 once 3/4 are using overlays (or unknown).
    return composition_count < (PresentationHistory::kPresentsToStore / 4);
  }
}

bool DCLayerTree::SwapChainPresenter::UploadVideoImages(
    gl::GLImageMemory* y_image_memory,
    gl::GLImageMemory* uv_image_memory) {
  gfx::Size texture_size = y_image_memory->GetSize();
  gfx::Size uv_image_size = uv_image_memory->GetSize();
  if (uv_image_size.height() != texture_size.height() / 2 ||
      uv_image_size.width() != texture_size.width() / 2 ||
      y_image_memory->format() != gfx::BufferFormat::R_8 ||
      uv_image_memory->format() != gfx::BufferFormat::RG_88) {
    DVLOG(ERROR) << "Invalid NV12 GLImageMemory properties.";
    return false;
  }

  if (!staging_texture_ || (staging_texture_size_ != texture_size)) {
    staging_texture_.Reset();
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texture_size.width();
    desc.Height = texture_size.height();
    desc.Format = DXGI_FORMAT_NV12;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;

    // This isn't actually bound to a decoder, but dynamic textures need
    // BindFlags to be nonzero and D3D11_BIND_DECODER also works when creating
    // a VideoProcessorInputView.
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d11_device_->CreateTexture2D(
        &desc, nullptr, staging_texture_.GetAddressOf());
    CHECK(SUCCEEDED(hr)) << "Creating D3D11 video upload texture failed: "
                         << std::hex << hr;
    staging_texture_size_ = texture_size;
  }
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  HRESULT hr = context->Map(staging_texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                            0, &mapped_resource);
  CHECK(SUCCEEDED(hr)) << "Mapping D3D11 video upload texture failed: "
                       << std::hex << hr;

  size_t dest_stride = mapped_resource.RowPitch;
  for (int y = 0; y < texture_size.height(); y++) {
    const uint8_t* y_source =
        y_image_memory->memory() + y * y_image_memory->stride();
    uint8_t* dest =
        reinterpret_cast<uint8_t*>(mapped_resource.pData) + dest_stride * y;
    memcpy(dest, y_source, texture_size.width());
  }

  uint8_t* uv_dest_plane_start =
      reinterpret_cast<uint8_t*>(mapped_resource.pData) +
      dest_stride * texture_size.height();
  for (int y = 0; y < uv_image_size.height(); y++) {
    const uint8_t* uv_source =
        uv_image_memory->memory() + y * uv_image_memory->stride();
    uint8_t* dest = uv_dest_plane_start + dest_stride * y;
    memcpy(dest, uv_source, texture_size.width());
  }
  context->Unmap(staging_texture_.Get(), 0);
  return true;
}

void DCLayerTree::SwapChainPresenter::PresentToSwapChain(
    const ui::DCRendererLayerParams& params) {
  gl::GLImageDXGIBase* image_dxgi =
      gl::GLImageDXGIBase::FromGLImage(params.image[0].get());
  gl::GLImageMemory* y_image_memory = nullptr;
  gl::GLImageMemory* uv_image_memory = nullptr;
  if (params.image.size() >= 2) {
    y_image_memory = gl::GLImageMemory::FromGLImage(params.image[0].get());
    uv_image_memory = gl::GLImageMemory::FromGLImage(params.image[1].get());
  }

  if (!image_dxgi && (!y_image_memory || !uv_image_memory)) {
    DLOG(ERROR) << "Video GLImages are missing";
    last_gl_images_.clear();
    return;
  }

  // Swap chain size is the minimum of the on-screen size and the source
  // size so the video processor can do the minimal amount of work and
  // the overlay has to read the minimal amount of data.
  // DWM is also less likely to promote a surface to an overlay if it's
  // much larger than its area on-screen.
  gfx::Rect bounds_rect = params.rect;
  gfx::Size ceiled_input_size = gfx::ToCeiledSize(params.contents_rect.size());
  gfx::Size swap_chain_size = bounds_rect.size();

  if (g_supports_scaled_overlays)
    swap_chain_size.SetToMin(ceiled_input_size);

  // YUY2 surfaces must have an even width.
  if (swap_chain_size.width() % 2 == 1)
    swap_chain_size.set_width(swap_chain_size.width() + 1);

  InitializeVideoProcessor(ceiled_input_size, swap_chain_size);

  if (surface_->workarounds().disable_larger_than_screen_overlays) {
    // Because of the rounding when converting between pixels and DIPs, a
    // fullscreen video can become slightly larger than the monitor - e.g. on
    // a 3000x2000 monitor with a scale factor of 1.75 a 1920x1079 video can
    // become 3002x1689.
    // On older Intel drivers, swapchains that are bigger than the monitor
    // won't be put into overlays, which will hurt power usage a lot. On those
    // systems, the scaling can be adjusted very slightly so that it's less
    // than the monitor size. This should be close to imperceptible.
    // TODO(jbauman): Remove when http://crbug.com/668278 is fixed.
    const int kOversizeMargin = 3;

    if ((bounds_rect.x() >= 0) &&
        (bounds_rect.width() > g_overlay_monitor_size.width()) &&
        (bounds_rect.width() <=
         g_overlay_monitor_size.width() + kOversizeMargin)) {
      bounds_rect.set_width(g_overlay_monitor_size.width());
    }

    if ((bounds_rect.y() >= 0) &&
        (bounds_rect.height() > g_overlay_monitor_size.height()) &&
        (bounds_rect.height() <=
         g_overlay_monitor_size.height() + kOversizeMargin)) {
      bounds_rect.set_height(g_overlay_monitor_size.height());
    }
  }

  swap_chain_scale_x_ = bounds_rect.width() * 1.0f / swap_chain_size.width();
  swap_chain_scale_y_ = bounds_rect.height() * 1.0f / swap_chain_size.height();

  bool yuy2_swapchain = ShouldBeYUY2();
  bool first_present = false;
  if (!swap_chain_ || swap_chain_size_ != swap_chain_size ||
      ((yuy2_swapchain != is_yuy2_swapchain_) &&
       !failed_to_create_yuy2_swapchain_)) {
    first_present = true;
    swap_chain_size_ = swap_chain_size;
    swap_chain_.Reset();
    ReallocateSwapChain(yuy2_swapchain);
  } else if (last_gl_images_ == params.image) {
    // The swap chain is presenting the same images as last swap, which means
    // that the images were never returned to the video decoder and should
    // have the same contents as last time. It shouldn't need to be redrawn.
    return;
  }

  last_gl_images_ = params.image;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  UINT input_level;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  if (image_dxgi) {
    input_texture = image_dxgi->texture();
    input_level = (UINT)image_dxgi->level();
    if (!input_texture)
      return;
    // Keyed mutex may not exist.
    keyed_mutex = image_dxgi->keyed_mutex();
    staging_texture_.Reset();
  } else {
    DCHECK(y_image_memory);
    DCHECK(uv_image_memory);
    if (!UploadVideoImages(y_image_memory, uv_image_memory))
      return;
    DCHECK(staging_texture_);
    input_texture = staging_texture_;
    input_level = 0;
  }

  if (!out_view_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(texture.GetAddressOf()));
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC out_desc = {};
    out_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    out_desc.Texture2D.MipSlice = 0;
    HRESULT hr = video_device_->CreateVideoProcessorOutputView(
        texture.Get(), video_processor_enumerator_.Get(), &out_desc,
        out_view_.GetAddressOf());
    CHECK(SUCCEEDED(hr));
  }

  // TODO(jbauman): Use correct colorspace.
  gfx::ColorSpace src_color_space = gfx::ColorSpace::CreateREC709();
  if (image_dxgi && image_dxgi->color_space().IsValid()) {
    src_color_space = image_dxgi->color_space();
  }
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> context1;
  if (SUCCEEDED(video_context_.CopyTo(context1.GetAddressOf()))) {
    context1->VideoProcessorSetStreamColorSpace1(
        video_processor_.Get(), 0,
        gfx::ColorSpaceWin::GetDXGIColorSpace(src_color_space));
  } else {
    // This can't handle as many different types of color spaces, so use it
    // only if ID3D11VideoContext1 isn't available.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
    video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                      &color_space);
  }

  gfx::ColorSpace output_color_space =
      is_yuy2_swapchain_ ? src_color_space : gfx::ColorSpace::CreateSRGB();
  if (base::FeatureList::IsEnabled(kFallbackBT709VideoToBT601) &&
      (output_color_space == gfx::ColorSpace::CreateREC709())) {
    output_color_space = gfx::ColorSpace::CreateREC601();
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
  if (SUCCEEDED(swap_chain_.CopyTo(swap_chain3.GetAddressOf()))) {
    DXGI_COLOR_SPACE_TYPE color_space =
        gfx::ColorSpaceWin::GetDXGIColorSpace(output_color_space);
    if (is_yuy2_swapchain_) {
      // Swapchains with YUY2 textures can't have RGB color spaces.
      switch (color_space) {
        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
        case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
          color_space = DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;
          break;

        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709:
          color_space = DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
          break;

        case DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020:
          color_space = DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
          break;

        case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
        case DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020:
          color_space = DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
          break;

        case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
          color_space = DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020;
          break;

        default:
          break;
      }
    }
    HRESULT hr = swap_chain3->SetColorSpace1(color_space);

    if (SUCCEEDED(hr)) {
      if (context1) {
        context1->VideoProcessorSetOutputColorSpace1(video_processor_.Get(),
                                                     color_space);
      } else {
        D3D11_VIDEO_PROCESSOR_COLOR_SPACE d3d11_color_space =
            gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
        video_context_->VideoProcessorSetOutputColorSpace(
            video_processor_.Get(), &d3d11_color_space);
      }
    }
  }

  {
    if (keyed_mutex) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      const int kMaxSyncTimeMs = 1000;
      HRESULT hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Error acquiring keyed mutex: " << std::hex << hr;
        return;
      }
    }
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC in_desc = {};
    in_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    in_desc.Texture2D.ArraySlice = input_level;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> in_view;
    HRESULT hr = video_device_->CreateVideoProcessorInputView(
        input_texture.Get(), video_processor_enumerator_.Get(), &in_desc,
        in_view.GetAddressOf());
    CHECK(SUCCEEDED(hr));

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = true;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = in_view.Get();
    RECT dest_rect = gfx::Rect(swap_chain_size).ToRECT();
    video_context_->VideoProcessorSetOutputTargetRect(video_processor_.Get(),
                                                      TRUE, &dest_rect);
    video_context_->VideoProcessorSetStreamDestRect(video_processor_.Get(), 0,
                                                    TRUE, &dest_rect);
    RECT source_rect = gfx::Rect(ceiled_input_size).ToRECT();
    video_context_->VideoProcessorSetStreamSourceRect(video_processor_.Get(), 0,
                                                      TRUE, &source_rect);

    hr = video_context_->VideoProcessorBlt(video_processor_.Get(),
                                           out_view_.Get(), 0, 1, &stream);
    CHECK(SUCCEEDED(hr));
    if (keyed_mutex) {
      HRESULT hr = keyed_mutex->ReleaseSync(0);
      DCHECK(SUCCEEDED(hr));
    }
  }

  if (first_present) {
    swap_chain_->Present(0, 0);

    // DirectComposition can display black for a swapchain between the first
    // and second time it's presented to - maybe the first Present can get
    // lost somehow and it shows the wrong buffer. In that case copy the
    // buffers so both have the correct contents, which seems to help. The
    // first Present() after this needs to have SyncInterval > 0, or else the
    // workaround doesn't help.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> dest_texture;
    HRESULT hr =
        swap_chain_->GetBuffer(0, IID_PPV_ARGS(dest_texture.GetAddressOf()));
    DCHECK(SUCCEEDED(hr));
    Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture;
    hr = swap_chain_->GetBuffer(1, IID_PPV_ARGS(src_texture.GetAddressOf()));
    DCHECK(SUCCEEDED(hr));
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device_->GetImmediateContext(context.GetAddressOf());
    context->CopyResource(dest_texture.Get(), src_texture.Get());

    // Additionally wait for the GPU to finish executing its commands, or
    // there still may be a black flicker when presenting expensive content
    // (e.g. 4k video).
    Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
    hr = d3d11_device_.CopyTo(dxgi_device2.GetAddressOf());
    DCHECK(SUCCEEDED(hr));
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    dxgi_device2->EnqueueSetEvent(event.handle());
    event.Wait();
  }

  swap_chain_->Present(1, 0);

  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.SwapchainFormat",
                        is_yuy2_swapchain_);
  frames_since_color_space_change_++;

  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media;
  if (SUCCEEDED(swap_chain_.CopyTo(swap_chain_media.GetAddressOf()))) {
    DXGI_FRAME_STATISTICS_MEDIA stats = {};
    if (SUCCEEDED(swap_chain_media->GetFrameStatisticsMedia(&stats))) {
      base::UmaHistogramSparse("GPU.DirectComposition.CompositionMode",
                               stats.CompositionMode);
      presentation_history_.AddSample(stats.CompositionMode);
    }
  }
}

bool DCLayerTree::SwapChainPresenter::InitializeVideoProcessor(
    const gfx::Size& in_size,
    const gfx::Size& out_size) {
  if (video_processor_ && SizeContains(processor_input_size_, in_size) &&
      SizeContains(processor_output_size_, out_size))
    return false;
  processor_input_size_ = in_size;
  processor_output_size_ = out_size;
  surface_->InitializeVideoProcessor(in_size, out_size);

  video_processor_enumerator_ = surface_->video_processor_enumerator();
  video_processor_ = surface_->video_processor();
  // out_view_ depends on video_processor_enumerator_, so ensure it's
  // recreated if the enumerator is.
  out_view_.Reset();
  return true;
}

void DCLayerTree::SwapChainPresenter::ReallocateSwapChain(bool yuy2) {
  TRACE_EVENT0("gpu", "DCLayerTree::SwapChainPresenter::ReallocateSwapChain");
  DCHECK(!swap_chain_);

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device_.CopyTo(dxgi_device.GetAddressOf());
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());
  Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf()));

  Microsoft::WRL::ComPtr<IDXGIFactoryMedia> media_factory;
  dxgi_factory.CopyTo(media_factory.GetAddressOf());
  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = swap_chain_size_.width();
  desc.Height = swap_chain_size_.height();
  desc.Format = DXGI_FORMAT_YUY2;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags =
      DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO | DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;

  HANDLE handle;
  HRESULT hr = create_surface_handle_function_(COMPOSITIONOBJECT_ALL_ACCESS,
                                               nullptr, &handle);
  // TODO(crbug/792806): Remove Alias and CHECK after issue is fixed.
  base::debug::Alias(&hr);
  CHECK(SUCCEEDED(hr));
  swap_chain_handle_.Set(handle);

  if (is_yuy2_swapchain_ != yuy2) {
    UMA_HISTOGRAM_COUNTS_1000(
        "GPU.DirectComposition.FramesSinceColorSpaceChange",
        frames_since_color_space_change_);
  }

  frames_since_color_space_change_ = 0;

  is_yuy2_swapchain_ = false;
  // The composition surface handle isn't actually used, but
  // CreateSwapChainForComposition can't create YUY2 swapchains.
  if (yuy2) {
    hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        swap_chain_.GetAddressOf());
    is_yuy2_swapchain_ = SUCCEEDED(hr);
    failed_to_create_yuy2_swapchain_ = !is_yuy2_swapchain_;
  }

  if (!is_yuy2_swapchain_) {
    if (yuy2) {
      DLOG(ERROR) << "YUY2 creation failed with " << std::hex << hr
                  << ". Falling back to BGRA";
    }
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Flags = 0;
    hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        swap_chain_.GetAddressOf());
    CHECK(SUCCEEDED(hr));
  }
  out_view_.Reset();
}

bool DCLayerTree::InitVisual(size_t i) {
  DCHECK_GT(visual_info_.size(), i);
  VisualInfo* visual_info = &visual_info_[i];
  if (visual_info->content_visual)
    return false;
  DCHECK(!visual_info->clip_visual);
  Microsoft::WRL::ComPtr<IDCompositionVisual2> visual;
  dcomp_device_->CreateVisual(visual_info->clip_visual.GetAddressOf());
  dcomp_device_->CreateVisual(visual.GetAddressOf());
  visual_info->content_visual = visual;
  visual_info->clip_visual->AddVisual(visual.Get(), FALSE, nullptr);

  IDCompositionVisual2* last_visual =
      (i > 0) ? visual_info_[i - 1].clip_visual.Get() : nullptr;
  root_visual_->AddVisual(visual_info->clip_visual.Get(), TRUE, last_visual);
  return true;
}

bool DCLayerTree::UpdateVisualForVideo(
    VisualInfo* visual_info,
    const ui::DCRendererLayerParams& params) {
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dc_visual =
      visual_info->content_visual;

  bool changed = false;
  gfx::Rect bounds_rect = params.rect;
  visual_info->surface.Reset();
  if (!visual_info->swap_chain_presenter) {
    visual_info->swap_chain_presenter =
        std::make_unique<SwapChainPresenter>(this, d3d11_device_);
  }
  visual_info->swap_chain_presenter->PresentToSwapChain(params);
  if (visual_info->swap_chain !=
      visual_info->swap_chain_presenter->swap_chain()) {
    visual_info->swap_chain = visual_info->swap_chain_presenter->swap_chain();
    dc_visual->SetContent(visual_info->swap_chain.Get());
    changed = true;
  }

  if (visual_info->swap_chain_presenter->swap_chain_scale_x() !=
          visual_info->swap_chain_scale_x ||
      visual_info->swap_chain_presenter->swap_chain_scale_y() !=
          visual_info->swap_chain_scale_y ||
      params.transform != visual_info->transform ||
      visual_info->bounds != bounds_rect) {
    visual_info->swap_chain_scale_x =
        visual_info->swap_chain_presenter->swap_chain_scale_x();
    visual_info->swap_chain_scale_y =
        visual_info->swap_chain_presenter->swap_chain_scale_y();
    visual_info->transform = params.transform;
    visual_info->bounds = bounds_rect;

    gfx::Transform final_transform = params.transform;
    gfx::Transform scale_transform;
    scale_transform.Scale(
        visual_info->swap_chain_presenter->swap_chain_scale_x(),
        visual_info->swap_chain_presenter->swap_chain_scale_y());
    final_transform.PreconcatTransform(scale_transform);
    final_transform.Transpose();

    dc_visual->SetOffsetX(bounds_rect.x());
    dc_visual->SetOffsetY(bounds_rect.y());
    Microsoft::WRL::ComPtr<IDCompositionMatrixTransform> dcomp_transform;
    dcomp_device_->CreateMatrixTransform(dcomp_transform.GetAddressOf());
    D2D_MATRIX_3X2_F d2d_matrix = {{{final_transform.matrix().get(0, 0),
                                     final_transform.matrix().get(0, 1),
                                     final_transform.matrix().get(1, 0),
                                     final_transform.matrix().get(1, 1),
                                     final_transform.matrix().get(3, 0),
                                     final_transform.matrix().get(3, 1)}}};
    dcomp_transform->SetMatrix(d2d_matrix);
    dc_visual->SetTransform(dcomp_transform.Get());
    changed = true;
  }
  return changed;
}

bool DCLayerTree::UpdateVisualForBackbuffer(
    VisualInfo* visual_info,
    const ui::DCRendererLayerParams& params) {
  Microsoft::WRL::ComPtr<IDCompositionVisual2> dc_visual =
      visual_info->content_visual;

  visual_info->swap_chain_presenter = nullptr;
  bool changed = false;
  if ((visual_info->surface != surface_->dcomp_surface()) ||
      (visual_info->swap_chain != surface_->swap_chain())) {
    visual_info->surface = surface_->dcomp_surface();
    visual_info->swap_chain = surface_->swap_chain();
    if (visual_info->surface) {
      dc_visual->SetContent(visual_info->surface.Get());
    } else if (visual_info->swap_chain) {
      dc_visual->SetContent(visual_info->swap_chain.Get());
    } else {
      dc_visual->SetContent(nullptr);
    }
    changed = true;
  }

  gfx::Rect bounds_rect = params.rect;
  if (visual_info->bounds != bounds_rect ||
      !visual_info->transform.IsIdentity()) {
    dc_visual->SetOffsetX(bounds_rect.x());
    dc_visual->SetOffsetY(bounds_rect.y());
    visual_info->bounds = bounds_rect;
    dc_visual->SetTransform(nullptr);
    visual_info->transform = gfx::Transform();
    changed = true;
  }
  if (surface_->dcomp_surface() &&
      surface_->GetDCompSurfaceSerial() != visual_info->dcomp_surface_serial) {
    changed = true;
    visual_info->dcomp_surface_serial = surface_->GetDCompSurfaceSerial();
  }
  return changed;
}

bool DCLayerTree::UpdateVisualClip(VisualInfo* visual_info,
                                   const ui::DCRendererLayerParams& params) {
  if (params.is_clipped != visual_info->is_clipped ||
      params.clip_rect != visual_info->clip_rect) {
    // DirectComposition clips happen in the pre-transform visual
    // space, while cc/ clips happen post-transform. So the clip needs
    // to go on a separate parent visual that's untransformed.
    visual_info->is_clipped = params.is_clipped;
    visual_info->clip_rect = params.clip_rect;
    if (params.is_clipped) {
      Microsoft::WRL::ComPtr<IDCompositionRectangleClip> clip;
      dcomp_device_->CreateRectangleClip(clip.GetAddressOf());
      gfx::Rect offset_clip = params.clip_rect;
      clip->SetLeft(offset_clip.x());
      clip->SetRight(offset_clip.right());
      clip->SetBottom(offset_clip.bottom());
      clip->SetTop(offset_clip.y());
      visual_info->clip_visual->SetClip(clip.Get());
    } else {
      visual_info->clip_visual->SetClip(nullptr);
    }
    return true;
  }
  return false;
}

bool DCLayerTree::CommitAndClearPendingOverlays() {
  TRACE_EVENT1("gpu", "DCLayerTree::CommitAndClearPendingOverlays", "size",
               pending_overlays_.size());
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlaysUsed",
                        !pending_overlays_.empty());
  // Add an overlay with z-order 0 representing the main plane.
  gfx::Size surface_size = surface_->GetSize();
  pending_overlays_.push_back(std::make_unique<ui::DCRendererLayerParams>(
      false, gfx::Rect(), 0, gfx::Transform(),
      std::vector<scoped_refptr<gl::GLImage>>(),
      gfx::RectF(gfx::SizeF(surface_size)), gfx::Rect(surface_size), 0, 0, 1.0,
      0));

  // TODO(jbauman): Reuse swapchains that are switched between overlays and
  // underlays.
  std::sort(pending_overlays_.begin(), pending_overlays_.end(),
            [](const auto& a, const auto& b) -> bool {
              return a->z_order < b->z_order;
            });

  bool changed = false;
  while (visual_info_.size() > pending_overlays_.size()) {
    visual_info_.back().clip_visual->RemoveAllVisuals();
    root_visual_->RemoveVisual(visual_info_.back().clip_visual.Get());
    visual_info_.pop_back();
    changed = true;
  }

  visual_info_.resize(pending_overlays_.size());

  // The overall visual tree has one clip visual for every overlay (including
  // the main plane). The clip visuals are in z_order and are all children of
  // a root visual. Each clip visual has a child visual that has the actual
  // plane content.

  for (size_t i = 0; i < pending_overlays_.size(); i++) {
    ui::DCRendererLayerParams& params = *pending_overlays_[i];
    VisualInfo* visual_info = &visual_info_[i];

    changed |= InitVisual(i);
    if (params.image.size() >= 1 && params.image[0]) {
      changed |= UpdateVisualForVideo(visual_info, params);
    } else if (params.image.empty()) {
      changed |= UpdateVisualForBackbuffer(visual_info, params);
    } else {
      CHECK(false);
    }
    changed |= UpdateVisualClip(visual_info, params);
  }

  if (changed) {
    HRESULT hr = dcomp_device_->Commit();
    CHECK(SUCCEEDED(hr));
  }

  pending_overlays_.clear();
  return true;
}

bool DCLayerTree::ScheduleDCLayer(const ui::DCRendererLayerParams& params) {
  pending_overlays_.push_back(
      std::make_unique<ui::DCRendererLayerParams>(params));
  return true;
}

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    std::unique_ptr<gfx::VSyncProvider> vsync_provider,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::GLSurfaceEGL(),
      child_window_(delegate, parent_window),
      workarounds_(delegate->GetFeatureInfo()->workarounds()),
      vsync_provider_(std::move(vsync_provider)) {}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

// static
bool DirectCompositionSurfaceWin::AreOverlaysSupported() {
  static bool initialized;
  static bool overlays_supported;
  if (initialized)
    return overlays_supported;

  initialized = true;

  overlays_supported =
      HardwareSupportsOverlays() &&
      base::FeatureList::IsEnabled(switches::kDirectCompositionOverlays);

  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlaysSupported",
                        overlays_supported);
  return overlays_supported;
}

// static
bool DirectCompositionSurfaceWin::IsHDRSupported() {
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS2)
    return false;

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory> factory;
  hr = CreateDXGIFactory(__uuidof(IDXGIFactory),
                         reinterpret_cast<void**>(factory.GetAddressOf()));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DXGI factory.";
    return false;
  }

  bool hdr_monitor_found = false;
  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, adapter.GetAddressOf());
    if (hr == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, output.GetAddressOf());
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(
          __uuidof(IDXGIOutput6),
          reinterpret_cast<void**>(output6.GetAddressOf()));
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

bool DirectCompositionSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;

  bool result = child_window_.Initialize();
  window_ = child_window_.window();
  return result;
}

bool DirectCompositionSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_)
    return false;

  EGLDisplay display = GetDisplay();
  if (!window_) {
    if (!InitializeNativeWindow()) {
      DLOG(ERROR) << "Failed to initialize native window";
      return false;
    }
  }

  layer_tree_ =
      std::make_unique<DCLayerTree>(this, d3d11_device_, dcomp_device_);
  if (!layer_tree_->Initialize(window_))
    return false;

  std::vector<EGLint> pbuffer_attribs;
  pbuffer_attribs.push_back(EGL_WIDTH);
  pbuffer_attribs.push_back(1);
  pbuffer_attribs.push_back(EGL_HEIGHT);
  pbuffer_attribs.push_back(1);

  pbuffer_attribs.push_back(EGL_NONE);
  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), &pbuffer_attribs[0]);
  if (!default_surface_) {
    LOG(ERROR) << "eglCreatePbufferSurface failed with error "
               << ui::GetLastEGLErrorString();
    return false;
  }

  if (!RecreateRootSurface())
    return false;

  presentation_helper_ =
      std::make_unique<gl::GLSurfacePresentationHelper>(vsync_provider_.get());
  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  presentation_helper_ = nullptr;
  if (default_surface_) {
    if (!eglDestroySurface(GetDisplay(), default_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    default_surface_ = nullptr;
  }
  if (root_surface_)
    root_surface_->Destroy();
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return root_surface_ ? root_surface_->GetHandle() : default_surface_;
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         ColorSpace color_space,
                                         bool has_alpha) {
  bool is_hdr = color_space == ColorSpace::SCRGB_LINEAR;
  if (size == GetSize() && has_alpha == has_alpha_ && is_hdr == is_hdr_)
    return true;

  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  size_ = size;
  is_hdr_ = is_hdr;
  has_alpha_ = has_alpha;
  return RecreateRootSurface();
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    const PresentationCallback& callback) {
  gl::GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), callback);
  ui::ScopedReleaseCurrent release_current;
  root_surface_->SwapBuffers(PresentationCallback());
  layer_tree_->CommitAndClearPendingOverlays();
  child_window_.ClearInvalidContents();
  if (!release_current.Restore())
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_FAILED);
  return scoped_swap_buffers.result();
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    const PresentationCallback& callback) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(callback);
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return vsync_provider_.get();
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    const ui::DCRendererLayerParams& params) {
  return layer_tree_->ScheduleDCLayer(params);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  if (enable_dc_layers_ == enable)
    return true;
  enable_dc_layers_ = enable;
  return RecreateRootSurface();
}

bool DirectCompositionSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  if (root_surface_)
    return root_surface_->OnMakeCurrent(context);
  return true;
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::UseOverlaysForVideo() const {
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  if (root_surface_)
    return root_surface_->SetDrawRectangle(rectangle);
  return false;
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  if (root_surface_)
    return root_surface_->GetDrawOffset();
  return gfx::Vector2d();
}

void DirectCompositionSurfaceWin::WaitForSnapshotRendering() {
  DCHECK(gl::GLContext::GetCurrent()->IsCurrent(this));
  glFinish();
}

bool DirectCompositionSurfaceWin::RecreateRootSurface() {
  root_surface_ = new DirectCompositionChildSurfaceWin(
      size_, is_hdr_, has_alpha_, enable_dc_layers_);
  return root_surface_->Initialize();
}

const Microsoft::WRL::ComPtr<IDCompositionSurface>
DirectCompositionSurfaceWin::dcomp_surface() const {
  return root_surface_ ? root_surface_->dcomp_surface() : nullptr;
}

const Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::swap_chain() const {
  return root_surface_ ? root_surface_->swap_chain() : nullptr;
}

uint64_t DirectCompositionSurfaceWin::GetDCompSurfaceSerial() const {
  return root_surface_ ? root_surface_->dcomp_surface_serial() : 0;
}

scoped_refptr<base::TaskRunner>
DirectCompositionSurfaceWin::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);
}

}  // namespace gpu
