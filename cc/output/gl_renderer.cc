// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/gl_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/context_provider.h"
#include "cc/output/geometry_binding.h"
#include "cc/output/gl_frame_data.h"
#include "cc/output/output_surface.h"
#include "cc/output/render_surface_filters.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/layer_quad.h"
#include "cc/resources/priority_calculator.h"
#include "cc/resources/scoped_resource.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/proxy.h"
#include "cc/trees/single_thread_proxy.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "third_party/skia/include/gpu/SkGrTexturePixelRef.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

using WebKit::WebGraphicsContext3D;
using WebKit::WebGraphicsMemoryAllocation;

namespace cc {

namespace {

// TODO(epenner): This should probably be moved to output surface.
//
// This implements a simple fence based on client side swaps.
// This is to isolate the ResourceProvider from 'frames' which
// it shouldn't need to care about, while still allowing us to
// enforce good texture recycling behavior strictly throughout
// the compositor (don't recycle a texture while it's in use).
class SimpleSwapFence : public ResourceProvider::Fence {
 public:
  SimpleSwapFence() : has_passed_(false) {}
  virtual bool HasPassed() OVERRIDE { return has_passed_; }
  void SetHasPassed() { has_passed_ = true; }
 private:
  virtual ~SimpleSwapFence() {}
  bool has_passed_;
};

bool NeedsIOSurfaceReadbackWorkaround() {
#if defined(OS_MACOSX)
  // This isn't strictly required in DumpRenderTree-mode when Mesa is used,
  // but it doesn't seem to hurt.
  return true;
#else
  return false;
#endif
}

}  // anonymous namespace

scoped_ptr<GLRenderer> GLRenderer::Create(RendererClient* client,
                                          OutputSurface* output_surface,
                                          ResourceProvider* resource_provider) {
  scoped_ptr<GLRenderer> renderer(
      new GLRenderer(client, output_surface, resource_provider));
  if (!renderer->Initialize())
    return scoped_ptr<GLRenderer>();

  return renderer.Pass();
}

GLRenderer::GLRenderer(RendererClient* client,
                       OutputSurface* output_surface,
                       ResourceProvider* resource_provider)
    : DirectRenderer(client, resource_provider),
      offscreen_framebuffer_id_(0),
      shared_geometry_quad_(gfx::RectF(-0.5f, -0.5f, 1.0f, 1.0f)),
      output_surface_(output_surface),
      context_(output_surface->context3d()),
      is_viewport_changed_(false),
      is_backbuffer_discarded_(false),
      discard_backbuffer_when_not_visible_(false),
      is_using_bind_uniform_(false),
      visible_(true),
      is_scissor_enabled_(false) {
  DCHECK(context_);
}

bool GLRenderer::Initialize() {
  if (!context_->makeContextCurrent())
    return false;

  context_->setContextLostCallback(this);
  context_->pushGroupMarkerEXT("CompositorContext");

  std::string extensions_string =
      UTF16ToASCII(context_->getString(GL_EXTENSIONS));
  std::vector<std::string> extensions_list;
  base::SplitString(extensions_string, ' ', &extensions_list);
  std::set<std::string> extensions(extensions_list.begin(),
                                   extensions_list.end());

  if (Settings().accelerate_painting &&
      extensions.count("GL_EXT_texture_format_BGRA8888") &&
      extensions.count("GL_EXT_read_format_bgra"))
    capabilities_.using_accelerated_painting = true;
  else
    capabilities_.using_accelerated_painting = false;

  capabilities_.using_partial_swap =
      Settings().partial_swap_enabled &&
      extensions.count("GL_CHROMIUM_post_sub_buffer");

  // Use the SwapBuffers callback only with the threaded proxy.
  if (client_->HasImplThread())
    capabilities_.using_swap_complete_callback =
        extensions.count("GL_CHROMIUM_swapbuffers_complete_callback");
  if (capabilities_.using_swap_complete_callback)
    context_->setSwapBuffersCompleteCallbackCHROMIUM(this);

  capabilities_.using_set_visibility =
      extensions.count("GL_CHROMIUM_set_visibility");

  if (extensions.count("GL_CHROMIUM_iosurface"))
    DCHECK(extensions.count("GL_ARB_texture_rectangle"));

  capabilities_.using_gpu_memory_manager =
      extensions.count("GL_CHROMIUM_gpu_memory_manager") &&
      Settings().use_memory_management;
  if (capabilities_.using_gpu_memory_manager)
    context_->setMemoryAllocationChangedCallbackCHROMIUM(this);

  capabilities_.using_egl_image = extensions.count("GL_OES_EGL_image_external");

  capabilities_.max_texture_size = resource_provider_->max_texture_size();
  capabilities_.best_texture_format = resource_provider_->best_texture_format();

  // The updater can access textures while the GLRenderer is using them.
  capabilities_.allow_partial_texture_updates = true;

  // Check for texture fast paths. Currently we always use MO8 textures,
  // so we only need to avoid POT textures if we have an NPOT fast-path.
  capabilities_.avoid_pow2_textures =
      extensions.count("GL_CHROMIUM_fast_NPOT_MO8_textures");

  capabilities_.using_offscreen_context3d = true;

  is_using_bind_uniform_ =
      extensions.count("GL_CHROMIUM_bind_uniform_location");

  // Make sure scissoring starts as disabled.
  GLC(context_, context_->disable(GL_SCISSOR_TEST));
  DCHECK(!is_scissor_enabled_);

  if (!InitializeSharedObjects())
    return false;

  // Make sure the viewport and context gets initialized, even if it is to zero.
  ViewportChanged();
  return true;
}

GLRenderer::~GLRenderer() {
  context_->setSwapBuffersCompleteCallbackCHROMIUM(NULL);
  context_->setMemoryAllocationChangedCallbackCHROMIUM(NULL);
  context_->setContextLostCallback(NULL);
  CleanupSharedObjects();
}

const RendererCapabilities& GLRenderer::Capabilities() const {
  return capabilities_;
}

WebGraphicsContext3D* GLRenderer::Context() { return context_; }

void GLRenderer::DebugGLCall(WebGraphicsContext3D* context,
                             const char* command,
                             const char* file,
                             int line) {
  unsigned long error = context->getError();
  if (error != GL_NO_ERROR)
    LOG(ERROR) << "GL command failed: File: " << file << "\n\tLine " << line
               << "\n\tcommand: " << command << ", error "
               << static_cast<int>(error) << "\n";
}

void GLRenderer::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;

  EnforceMemoryPolicy();

  // TODO: Replace setVisibilityCHROMIUM() with an extension to explicitly
  // manage front/backbuffers
  // crbug.com/116049
  if (capabilities_.using_set_visibility)
    context_->setVisibilityCHROMIUM(visible);
}

void GLRenderer::SendManagedMemoryStats(size_t bytes_visible,
                                        size_t bytes_visible_and_nearby,
                                        size_t bytes_allocated) {
  WebKit::WebGraphicsManagedMemoryStats stats;
  stats.bytesVisible = bytes_visible;
  stats.bytesVisibleAndNearby = bytes_visible_and_nearby;
  stats.bytesAllocated = bytes_allocated;
  stats.backbufferRequested = !is_backbuffer_discarded_;
  context_->sendManagedMemoryStatsCHROMIUM(&stats);
}

void GLRenderer::ReleaseRenderPassTextures() { render_pass_textures_.clear(); }

void GLRenderer::ViewportChanged() { is_viewport_changed_ = true; }

void GLRenderer::ClearFramebuffer(DrawingFrame* frame) {
  // On DEBUG builds, opaque render passes are cleared to blue to easily see
  // regions that were not drawn on the screen.
  if (frame->current_render_pass->has_transparent_background)
    GLC(context_, context_->clearColor(0, 0, 0, 0));
  else
    GLC(context_, context_->clearColor(0, 0, 1, 1));

#ifdef NDEBUG
  if (frame->current_render_pass->has_transparent_background)
#endif
    context_->clear(GL_COLOR_BUFFER_BIT);
}

void GLRenderer::BeginDrawingFrame(DrawingFrame* frame) {
  // FIXME: Remove this once backbuffer is automatically recreated on first use
  EnsureBackbuffer();

  if (ViewportSize().IsEmpty())
    return;

  TRACE_EVENT0("cc", "GLRenderer::DrawLayers");
  if (is_viewport_changed_) {
    // Only reshape when we know we are going to draw. Otherwise, the reshape
    // can leave the window at the wrong size if we never draw and the proper
    // viewport size is never set.
    is_viewport_changed_ = false;
    output_surface_->Reshape(gfx::Size(ViewportWidth(), ViewportHeight()));
  }

  MakeContextCurrent();
  // Bind the common vertex attributes used for drawing all the layers.
  shared_geometry_->PrepareForDraw();

  GLC(context_, context_->disable(GL_DEPTH_TEST));
  GLC(context_, context_->disable(GL_CULL_FACE));
  GLC(context_, context_->colorMask(true, true, true, true));
  GLC(context_, context_->enable(GL_BLEND));
  blend_shadow_ = true;
  GLC(context_, context_->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
  GLC(Context(), Context()->activeTexture(GL_TEXTURE0));
  program_shadow_ = 0;
}

void GLRenderer::DoNoOp() {
  GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, 0));
  GLC(context_, context_->flush());
}

void GLRenderer::DoDrawQuad(DrawingFrame* frame, const DrawQuad* quad) {
  DCHECK(quad->rect.Contains(quad->visible_rect));
  if (quad->material != DrawQuad::TEXTURE_CONTENT) {
    FlushTextureQuadCache();
  }

  switch (quad->material) {
    case DrawQuad::INVALID:
      NOTREACHED();
      break;
    case DrawQuad::CHECKERBOARD:
      DrawCheckerboardQuad(frame, CheckerboardDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(frame, DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::IO_SURFACE_CONTENT:
      DrawIOSurfaceQuad(frame, IOSurfaceDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(frame, RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(frame, SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawStreamVideoQuad(frame, StreamVideoDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      EnqueueTextureQuad(frame, TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileQuad(frame, TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      DrawYUVVideoQuad(frame, YUVVideoDrawQuad::MaterialCast(quad));
      break;
  }
}

void GLRenderer::DrawCheckerboardQuad(const DrawingFrame* frame,
                                      const CheckerboardDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  const TileCheckerboardProgram* program = GetTileCheckerboardProgram();
  DCHECK(program && (program->initialized() || IsContextLost()));
  SetUseProgram(program->program());

  SkColor color = quad->color;
  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().color_location(),
                           SkColorGetR(color) * (1.0f / 255.0f),
                           SkColorGetG(color) * (1.0f / 255.0f),
                           SkColorGetB(color) * (1.0f / 255.0f),
                           1));

  const int checkerboard_width = 16;
  float frequency = 1.0f / checkerboard_width;

  gfx::Rect tile_rect = quad->rect;
  float tex_offset_x = tile_rect.x() % checkerboard_width;
  float tex_offset_y = tile_rect.y() % checkerboard_width;
  float tex_scale_x = tile_rect.width();
  float tex_scale_y = tile_rect.height();
  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().tex_transform_location(),
                           tex_offset_x,
                           tex_offset_y,
                           tex_scale_x,
                           tex_scale_y));

  GLC(Context(),
      Context()->uniform1f(program->fragment_shader().frequency_location(),
                           frequency));

  SetShaderOpacity(quad->opacity(),
                   program->fragment_shader().alpha_location());
  DrawQuadGeometry(frame,
                   quad->quadTransform(),
                   quad->rect,
                   program->vertex_shader().matrix_location());
}

void GLRenderer::DrawDebugBorderQuad(const DrawingFrame* frame,
                                     const DebugBorderDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  static float gl_matrix[16];
  const DebugBorderProgram* program = GetDebugBorderProgram();
  DCHECK(program && (program->initialized() || IsContextLost()));
  SetUseProgram(program->program());

  // Use the full quad_rect for debug quads to not move the edges based on
  // partial swaps.
  gfx::Rect layer_rect = quad->rect;
  gfx::Transform render_matrix = quad->quadTransform();
  render_matrix.Translate(0.5f * layer_rect.width() + layer_rect.x(),
                          0.5f * layer_rect.height() + layer_rect.y());
  render_matrix.Scale(layer_rect.width(), layer_rect.height());
  GLRenderer::ToGLMatrix(&gl_matrix[0],
                         frame->projection_matrix * render_matrix);
  GLC(Context(),
      Context()->uniformMatrix4fv(
          program->vertex_shader().matrix_location(), 1, false, &gl_matrix[0]));

  SkColor color = quad->color;
  float alpha = SkColorGetA(color) * (1.0f / 255.0f);

  GLC(Context(),
      Context()->uniform4f(program->fragment_shader().color_location(),
                           (SkColorGetR(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetG(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetB(color) * (1.0f / 255.0f)) * alpha,
                           alpha));

  GLC(Context(), Context()->lineWidth(quad->width));

  // The indices for the line are stored in the same array as the triangle
  // indices.
  GLC(Context(),
      Context()->drawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0));
}

static inline SkBitmap ApplyFilters(GLRenderer* renderer,
                                    const WebKit::WebFilterOperations& filters,
                                    ScopedResource* source_texture_resource) {
  if (filters.isEmpty())
    return SkBitmap();

  ContextProvider* offscreen_contexts =
      renderer->resource_provider()->offscreen_context_provider();
  if (!offscreen_contexts || !offscreen_contexts->GrContext())
    return SkBitmap();

  ResourceProvider::ScopedWriteLockGL lock(renderer->resource_provider(),
                                           source_texture_resource->id());

  // Flush the compositor context to ensure that textures there are available
  // in the shared context.  Do this after locking/creating the compositor
  // texture.
  renderer->resource_provider()->Flush();

  // Make sure skia uses the correct GL context.
  offscreen_contexts->Context3d()->makeContextCurrent();

  SkBitmap source =
      RenderSurfaceFilters::Apply(filters,
                                  lock.texture_id(),
                                  source_texture_resource->size(),
                                  offscreen_contexts->GrContext());

  // Flush skia context so that all the rendered stuff appears on the
  // texture.
  offscreen_contexts->GrContext()->flush();

  // Flush the GL context so rendering results from this context are
  // visible in the compositor's context.
  offscreen_contexts->Context3d()->flush();

  // Use the compositor's GL context again.
  renderer->resource_provider()->GraphicsContext3D()->makeContextCurrent();
  return source;
}

static SkBitmap ApplyImageFilter(GLRenderer* renderer,
                                 SkImageFilter* filter,
                                 ScopedResource* source_texture_resource) {
  if (!filter)
    return SkBitmap();

  ContextProvider* offscreen_contexts =
      renderer->resource_provider()->offscreen_context_provider();
  if (!offscreen_contexts || !offscreen_contexts->GrContext())
    return SkBitmap();

  ResourceProvider::ScopedWriteLockGL lock(renderer->resource_provider(),
                                           source_texture_resource->id());

  // Flush the compositor context to ensure that textures there are available
  // in the shared context.  Do this after locking/creating the compositor
  // texture.
  renderer->resource_provider()->Flush();

  // Make sure skia uses the correct GL context.
  offscreen_contexts->Context3d()->makeContextCurrent();

  // Wrap the source texture in a Ganesh platform texture.
  GrBackendTextureDesc backend_texture_description;
  backend_texture_description.fWidth = source_texture_resource->size().width();
  backend_texture_description.fHeight =
      source_texture_resource->size().height();
  backend_texture_description.fConfig = kSkia8888_GrPixelConfig;
  backend_texture_description.fTextureHandle = lock.texture_id();
  backend_texture_description.fOrigin = kTopLeft_GrSurfaceOrigin;
  skia::RefPtr<GrTexture> texture =
      skia::AdoptRef(offscreen_contexts->GrContext()->wrapBackendTexture(
          backend_texture_description));

  // Place the platform texture inside an SkBitmap.
  SkBitmap source;
  source.setConfig(SkBitmap::kARGB_8888_Config,
                   source_texture_resource->size().width(),
                   source_texture_resource->size().height());
  skia::RefPtr<SkGrPixelRef> pixel_ref =
      skia::AdoptRef(new SkGrPixelRef(texture.get()));
  source.setPixelRef(pixel_ref.get());

  // Create a scratch texture for backing store.
  GrTextureDesc desc;
  desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
  desc.fSampleCnt = 0;
  desc.fWidth = source.width();
  desc.fHeight = source.height();
  desc.fConfig = kSkia8888_GrPixelConfig;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  GrAutoScratchTexture scratch_texture(
      offscreen_contexts->GrContext(), desc, GrContext::kExact_ScratchTexMatch);
  skia::RefPtr<GrTexture> backing_store =
      skia::AdoptRef(scratch_texture.detach());

  // Create a device and canvas using that backing store.
  SkGpuDevice device(offscreen_contexts->GrContext(), backing_store.get());
  SkCanvas canvas(&device);

  // Draw the source bitmap through the filter to the canvas.
  SkPaint paint;
  paint.setImageFilter(filter);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.drawSprite(source, 0, 0, &paint);

  // Flush skia context so that all the rendered stuff appears on the
  // texture.
  offscreen_contexts->GrContext()->flush();

  // Flush the GL context so rendering results from this context are
  // visible in the compositor's context.
  offscreen_contexts->Context3d()->flush();

  // Use the compositor's GL context again.
  renderer->resource_provider()->GraphicsContext3D()->makeContextCurrent();

  return device.accessBitmap(false);
}

scoped_ptr<ScopedResource> GLRenderer::DrawBackgroundFilters(
    DrawingFrame* frame,
    const RenderPassDrawQuad* quad,
    const gfx::Transform& contents_device_transform,
    const gfx::Transform& contents_device_transform_inverse) {
  // This method draws a background filter, which applies a filter to any pixels
  // behind the quad and seen through its background.  The algorithm works as
  // follows:
  // 1. Compute a bounding box around the pixels that will be visible through
  // the quad.
  // 2. Read the pixels in the bounding box into a buffer R.
  // 3. Apply the background filter to R, so that it is applied in the pixels'
  // coordinate space.
  // 4. Apply the quad's inverse transform to map the pixels in R into the
  // quad's content space. This implicitly clips R by the content bounds of the
  // quad since the destination texture has bounds matching the quad's content.
  // 5. Draw the background texture for the contents using the same transform as
  // used to draw the contents itself. This is done without blending to replace
  // the current background pixels with the new filtered background.
  // 6. Draw the contents of the quad over drop of the new background with
  // blending, as per usual. The filtered background pixels will show through
  // any non-opaque pixels in this draws.
  //
  // Pixel copies in this algorithm occur at steps 2, 3, 4, and 5.

  // FIXME: When this algorithm changes, update
  // LayerTreeHost::prioritizeTextures() accordingly.

  const WebKit::WebFilterOperations& filters = quad->background_filters;
  if (filters.isEmpty())
    return scoped_ptr<ScopedResource>();

  // FIXME: We only allow background filters on an opaque render surface because
  // other surfaces may contain translucent pixels, and the contents behind
  // those translucent pixels wouldn't have the filter applied.
  if (frame->current_render_pass->has_transparent_background)
    return scoped_ptr<ScopedResource>();
  DCHECK(!frame->current_texture);

  // FIXME: Do a single readback for both the surface and replica and cache the
  // filtered results (once filter textures are not reused).
  gfx::Rect device_rect = gfx::ToEnclosingRect(MathUtil::MapClippedRect(
      contents_device_transform, SharedGeometryQuad().BoundingBox()));

  int top, right, bottom, left;
  filters.getOutsets(top, right, bottom, left);
  device_rect.Inset(-left, -top, -right, -bottom);

  device_rect.Intersect(frame->current_render_pass->output_rect);

  scoped_ptr<ScopedResource> device_background_texture =
      ScopedResource::create(resource_provider_);
  if (!GetFramebufferTexture(device_background_texture.get(), device_rect))
    return scoped_ptr<ScopedResource>();

  SkBitmap filtered_device_background =
      ApplyFilters(this, filters, device_background_texture.get());
  if (!filtered_device_background.getTexture())
    return scoped_ptr<ScopedResource>();

  GrTexture* texture =
      reinterpret_cast<GrTexture*>(filtered_device_background.getTexture());
  int filtered_device_background_texture_id = texture->getTextureHandle();

  scoped_ptr<ScopedResource> background_texture =
      ScopedResource::create(resource_provider_);
  if (!background_texture->Allocate(quad->rect.size(),
                                    GL_RGBA,
                                    ResourceProvider::TextureUsageFramebuffer))
    return scoped_ptr<ScopedResource>();

  const RenderPass* target_render_pass = frame->current_render_pass;
  bool using_background_texture =
      UseScopedTexture(frame, background_texture.get(), quad->rect);

  if (using_background_texture) {
    // Copy the readback pixels from device to the background texture for the
    // surface.
    gfx::Transform device_to_framebuffer_transform;
    device_to_framebuffer_transform.Translate(
        quad->rect.width() * 0.5f + quad->rect.x(),
        quad->rect.height() * 0.5f + quad->rect.y());
    device_to_framebuffer_transform.Scale(quad->rect.width(),
                                          quad->rect.height());
    device_to_framebuffer_transform.PreconcatTransform(
        contents_device_transform_inverse);

#ifndef NDEBUG
    GLC(Context(), Context()->clearColor(0, 0, 1, 1));
    Context()->clear(GL_COLOR_BUFFER_BIT);
#endif

    CopyTextureToFramebuffer(frame,
                             filtered_device_background_texture_id,
                             device_rect,
                             device_to_framebuffer_transform);
  }

  UseRenderPass(frame, target_render_pass);

  if (!using_background_texture)
    return scoped_ptr<ScopedResource>();
  return background_texture.Pass();
}

void GLRenderer::DrawRenderPassQuad(DrawingFrame* frame,
                                    const RenderPassDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  CachedResource* contents_texture =
      render_pass_textures_.get(quad->render_pass_id);
  if (!contents_texture || !contents_texture->id())
    return;

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, quad->quadTransform(), quad->rect);
  gfx::Transform contents_device_transform =
      frame->window_matrix * frame->projection_matrix * quad_rect_matrix;
  contents_device_transform.FlattenTo2d();

  // Can only draw surface if device matrix is invertible.
  gfx::Transform contents_device_transform_inverse(
      gfx::Transform::kSkipInitialization);
  if (!contents_device_transform.GetInverse(&contents_device_transform_inverse))
    return;

  scoped_ptr<ScopedResource> background_texture =
      DrawBackgroundFilters(frame,
                            quad,
                            contents_device_transform,
                            contents_device_transform_inverse);

  // FIXME: Cache this value so that we don't have to do it for both the surface
  // and its replica.  Apply filters to the contents texture.
  SkBitmap filter_bitmap;
  if (quad->filter) {
    filter_bitmap =
        ApplyImageFilter(this, quad->filter.get(), contents_texture);
  } else {
    filter_bitmap = ApplyFilters(this, quad->filters, contents_texture);
  }

  // Draw the background texture if there is one.
  if (background_texture) {
    DCHECK(background_texture->size() == quad->rect.size());
    ResourceProvider::ScopedReadLockGL lock(resource_provider_,
                                            background_texture->id());
    CopyTextureToFramebuffer(
        frame, lock.texture_id(), quad->rect, quad->quadTransform());
  }

  bool clipped = false;
  gfx::QuadF device_quad = MathUtil::MapQuad(
      contents_device_transform, SharedGeometryQuad(), &clipped);
  DCHECK(!clipped);
  LayerQuad device_layer_bounds(gfx::QuadF(device_quad.BoundingBox()));
  LayerQuad device_layer_edges(device_quad);

  // Use anti-aliasing programs only when necessary.
  bool use_aa = (!device_quad.IsRectilinear() ||
                 !device_quad.BoundingBox().IsExpressibleAsRect());
  if (use_aa) {
    device_layer_bounds.InflateAntiAliasingDistance();
    device_layer_edges.InflateAntiAliasingDistance();
  }

  scoped_ptr<ResourceProvider::ScopedReadLockGL> mask_resource_lock;
  unsigned mask_texture_id = 0;
  if (quad->mask_resource_id) {
    mask_resource_lock.reset(new ResourceProvider::ScopedReadLockGL(
        resource_provider_, quad->mask_resource_id));
    mask_texture_id = mask_resource_lock->texture_id();
  }

  // FIXME: use the background_texture and blend the background in with this
  // draw instead of having a separate copy of the background texture.

  scoped_ptr<ResourceProvider::ScopedReadLockGL> contents_resource_lock;
  if (filter_bitmap.getTexture()) {
    GrTexture* texture =
        reinterpret_cast<GrTexture*>(filter_bitmap.getTexture());
    Context()->bindTexture(GL_TEXTURE_2D, texture->getTextureHandle());
  } else
    contents_resource_lock = make_scoped_ptr(
        new ResourceProvider::ScopedSamplerGL(resource_provider_,
                                              contents_texture->id(),
                                              GL_TEXTURE_2D,
                                              GL_LINEAR));

  int shader_quad_location = -1;
  int shader_edge_location = -1;
  int shader_mask_sampler_location = -1;
  int shader_mask_tex_coord_scale_location = -1;
  int shader_mask_tex_coord_offset_location = -1;
  int shader_matrix_location = -1;
  int shader_alpha_location = -1;
  int shader_tex_transform_location = -1;
  int shader_tex_scale_location = -1;

  if (use_aa && mask_texture_id) {
    const RenderPassMaskProgramAA* program = GetRenderPassMaskProgramAA();
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_quad_location = program->vertex_shader().point_location();
    shader_edge_location = program->fragment_shader().edge_location();
    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_scale_location = program->vertex_shader().tex_scale_location();
  } else if (!use_aa && mask_texture_id) {
    const RenderPassMaskProgram* program = GetRenderPassMaskProgram();
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_mask_sampler_location =
        program->fragment_shader().mask_sampler_location();
    shader_mask_tex_coord_scale_location =
        program->fragment_shader().mask_tex_coord_scale_location();
    shader_mask_tex_coord_offset_location =
        program->fragment_shader().mask_tex_coord_offset_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  } else if (use_aa && !mask_texture_id) {
    const RenderPassProgramAA* program = GetRenderPassProgramAA();
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_quad_location = program->vertex_shader().point_location();
    shader_edge_location = program->fragment_shader().edge_location();
    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_scale_location = program->vertex_shader().tex_scale_location();
  } else {
    const RenderPassProgram* program = GetRenderPassProgram();
    SetUseProgram(program->program());
    GLC(Context(),
        Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

    shader_matrix_location = program->vertex_shader().matrix_location();
    shader_alpha_location = program->fragment_shader().alpha_location();
    shader_tex_transform_location =
        program->vertex_shader().tex_transform_location();
  }

  float tex_scale_x =
      quad->rect.width() / static_cast<float>(contents_texture->size().width());
  float tex_scale_y = quad->rect.height() /
                      static_cast<float>(contents_texture->size().height());
  DCHECK_LE(tex_scale_x, 1.0f);
  DCHECK_LE(tex_scale_y, 1.0f);

  if (shader_tex_transform_location != -1) {
    GLC(Context(),
        Context()->uniform4f(shader_tex_transform_location,
                             0.0f,
                             0.0f,
                             tex_scale_x,
                             tex_scale_y));
  } else if (shader_tex_scale_location != -1) {
    GLC(Context(),
        Context()->uniform2f(
            shader_tex_scale_location, tex_scale_x, tex_scale_y));
  } else {
    DCHECK(IsContextLost());
  }

  if (shader_mask_sampler_location != -1) {
    DCHECK(shader_mask_tex_coord_scale_location != 1);
    DCHECK(shader_mask_tex_coord_offset_location != 1);
    GLC(Context(), Context()->activeTexture(GL_TEXTURE1));
    GLC(Context(), Context()->uniform1i(shader_mask_sampler_location, 1));
    GLC(Context(),
        Context()->uniform2f(shader_mask_tex_coord_offset_location,
                             quad->mask_uv_rect.x(),
                             quad->mask_uv_rect.y()));
    GLC(Context(),
        Context()->uniform2f(shader_mask_tex_coord_scale_location,
                             quad->mask_uv_rect.width() / tex_scale_x,
                             quad->mask_uv_rect.height() / tex_scale_y));
    resource_provider_->BindForSampling(
        quad->mask_resource_id, GL_TEXTURE_2D, GL_LINEAR);
    GLC(Context(), Context()->activeTexture(GL_TEXTURE0));
  }

  if (shader_edge_location != -1) {
    float edge[24];
    device_layer_edges.ToFloatArray(edge);
    device_layer_bounds.ToFloatArray(&edge[12]);
    GLC(Context(), Context()->uniform3fv(shader_edge_location, 8, edge));
  }

  // Map device space quad to surface space. contents_device_transform has no 3d
  // component since it was flattened, so we don't need to project.
  gfx::QuadF surface_quad = MathUtil::MapQuad(contents_device_transform_inverse,
                                              device_layer_edges.ToQuadF(),
                                              &clipped);
  DCHECK(!clipped);

  SetShaderOpacity(quad->opacity(), shader_alpha_location);
  SetShaderQuadF(surface_quad, shader_quad_location);
  DrawQuadGeometry(
      frame, quad->quadTransform(), quad->rect, shader_matrix_location);

  // Flush the compositor context before the filter bitmap goes out of
  // scope, so the draw gets processed before the filter texture gets deleted.
  if (filter_bitmap.getTexture())
    context_->flush();
}

struct SolidColorProgramUniforms {
  unsigned program;
  unsigned matrix_location;
  unsigned color_location;
  unsigned point_location;
  unsigned tex_scale_location;
  unsigned edge_location;
};

template<class T>
static void SolidColorUniformLocation(T program,
                                      SolidColorProgramUniforms* uniforms) {
  uniforms->program = program->program();
  uniforms->matrix_location = program->vertex_shader().matrix_location();
  uniforms->color_location = program->fragment_shader().color_location();
  uniforms->point_location = program->vertex_shader().point_location();
  uniforms->tex_scale_location = program->vertex_shader().tex_scale_location();
  uniforms->edge_location = program->fragment_shader().edge_location();
}

bool GLRenderer::SetupQuadForAntialiasing(
    const gfx::Transform& device_transform,
    const DrawQuad* quad,
    gfx::QuadF* local_quad,
    float edge[24]) const {
  gfx::Rect tile_rect = quad->visible_rect;

  bool clipped = false;
  gfx::QuadF device_layer_quad = MathUtil::MapQuad(
      device_transform, gfx::QuadF(quad->visibleContentRect()), &clipped);
  DCHECK(!clipped);

  // TODO(reveman): Axis-aligned is not enough to avoid anti-aliasing.
  // Bounding rectangle for quad also needs to be expressible as an integer
  // rectangle. crbug.com/169374
  bool is_axis_aligned_in_target = device_layer_quad.IsRectilinear();
  bool use_aa = !clipped && !is_axis_aligned_in_target && quad->IsEdge();

  if (!use_aa)
    return false;

  LayerQuad device_layer_bounds(gfx::QuadF(device_layer_quad.BoundingBox()));
  device_layer_bounds.InflateAntiAliasingDistance();

  LayerQuad device_layer_edges(device_layer_quad);
  device_layer_edges.InflateAntiAliasingDistance();

  device_layer_edges.ToFloatArray(edge);
  device_layer_bounds.ToFloatArray(&edge[12]);

  gfx::PointF bottom_right = tile_rect.bottom_right();
  gfx::PointF bottom_left = tile_rect.bottom_left();
  gfx::PointF top_left = tile_rect.origin();
  gfx::PointF top_right = tile_rect.top_right();

  // Map points to device space.
  bottom_right = MathUtil::MapPoint(device_transform, bottom_right, &clipped);
  DCHECK(!clipped);
  bottom_left = MathUtil::MapPoint(device_transform, bottom_left, &clipped);
  DCHECK(!clipped);
  top_left = MathUtil::MapPoint(device_transform, top_left, &clipped);
  DCHECK(!clipped);
  top_right = MathUtil::MapPoint(device_transform, top_right, &clipped);
  DCHECK(!clipped);

  LayerQuad::Edge bottom_edge(bottom_right, bottom_left);
  LayerQuad::Edge left_edge(bottom_left, top_left);
  LayerQuad::Edge top_edge(top_left, top_right);
  LayerQuad::Edge right_edge(top_right, bottom_right);

  // Only apply anti-aliasing to edges not clipped by culling or scissoring.
  if (quad->IsTopEdge() && tile_rect.y() == quad->rect.y())
    top_edge = device_layer_edges.top();
  if (quad->IsLeftEdge() && tile_rect.x() == quad->rect.x())
    left_edge = device_layer_edges.left();
  if (quad->IsRightEdge() && tile_rect.right() == quad->rect.right())
    right_edge = device_layer_edges.right();
  if (quad->IsBottomEdge() && tile_rect.bottom() == quad->rect.bottom())
    bottom_edge = device_layer_edges.bottom();

  float sign = gfx::QuadF(tile_rect).IsCounterClockwise() ? -1 : 1;
  bottom_edge.scale(sign);
  left_edge.scale(sign);
  top_edge.scale(sign);
  right_edge.scale(sign);

  // Create device space quad.
  LayerQuad device_quad(left_edge, top_edge, right_edge, bottom_edge);

  // Map device space quad to local space. device_transform has no 3d
  // component since it was flattened, so we don't need to project.  We should
  // have already checked that the transform was uninvertible above.
  gfx::Transform inverse_device_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert = device_transform.GetInverse(&inverse_device_transform);
  DCHECK(did_invert);
  *local_quad = MathUtil::MapQuad(
      inverse_device_transform, device_quad.ToQuadF(), &clipped);
  // We should not DCHECK(!clipped) here, because anti-aliasing inflation may
  // cause device_quad to become clipped. To our knowledge this scenario does
  // not need to be handled differently than the unclipped case.

  return true;
}

void GLRenderer::DrawSolidColorQuad(const DrawingFrame* frame,
                                    const SolidColorDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());
  gfx::Rect tile_rect = quad->visible_rect;

  gfx::Transform device_transform =
      frame->window_matrix * frame->projection_matrix * quad->quadTransform();
  device_transform.FlattenTo2d();
  if (!device_transform.IsInvertible())
    return;

  gfx::QuadF local_quad = gfx::QuadF(gfx::RectF(tile_rect));
  float edge[24];
  bool use_aa = SetupQuadForAntialiasing(
      device_transform, quad, &local_quad, edge);

  SolidColorProgramUniforms uniforms;
  if (use_aa)
    SolidColorUniformLocation(GetSolidColorProgramAA(), &uniforms);
  else
    SolidColorUniformLocation(GetSolidColorProgram(), &uniforms);
  SetUseProgram(uniforms.program);

  SkColor color = quad->color;
  float opacity = quad->opacity();
  float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;

  GLC(Context(),
      Context()->uniform4f(uniforms.color_location,
                           (SkColorGetR(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetG(color) * (1.0f / 255.0f)) * alpha,
                           (SkColorGetB(color) * (1.0f / 255.0f)) * alpha,
                           alpha));

  if (use_aa)
    GLC(Context(), Context()->uniform3fv(uniforms.edge_location, 8, edge));

  // Enable blending when the quad properties require it or if we decided
  // to use antialiasing.
  SetBlendEnabled(quad->ShouldDrawWithBlending() || use_aa);

  // Normalize to tile_rect.
  local_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());

  SetShaderQuadF(local_quad, uniforms.point_location);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  gfx::RectF centered_rect(gfx::PointF(-0.5f * tile_rect.width(),
                                       -0.5f * tile_rect.height()),
                           tile_rect.size());
  DrawQuadGeometry(frame, quad->quadTransform(),
                   centered_rect, uniforms.matrix_location);
}

struct TileProgramUniforms {
  unsigned program;
  unsigned sampler_location;
  unsigned vertex_tex_transform_location;
  unsigned fragment_tex_transform_location;
  unsigned edge_location;
  unsigned matrix_location;
  unsigned alpha_location;
  unsigned point_location;
};

template <class T>
static void TileUniformLocation(T program, TileProgramUniforms* uniforms) {
  uniforms->program = program->program();
  uniforms->vertex_tex_transform_location =
      program->vertex_shader().vertex_tex_transform_location();
  uniforms->matrix_location = program->vertex_shader().matrix_location();
  uniforms->point_location = program->vertex_shader().point_location();

  uniforms->sampler_location = program->fragment_shader().sampler_location();
  uniforms->alpha_location = program->fragment_shader().alpha_location();
  uniforms->fragment_tex_transform_location =
      program->fragment_shader().fragment_tex_transform_location();
  uniforms->edge_location = program->fragment_shader().edge_location();
}

void GLRenderer::DrawTileQuad(const DrawingFrame* frame,
                              const TileDrawQuad* quad) {
  gfx::Rect tile_rect = quad->visible_rect;

  gfx::RectF tex_coord_rect = quad->tex_coord_rect;
  float tex_to_geom_scale_x = quad->rect.width() / tex_coord_rect.width();
  float tex_to_geom_scale_y = quad->rect.height() / tex_coord_rect.height();

  // tex_coord_rect corresponds to quad_rect, but quad_visible_rect may be
  // smaller than quad_rect due to occlusion or clipping. Adjust
  // tex_coord_rect to match.
  gfx::Vector2d top_left_diff = tile_rect.origin() - quad->rect.origin();
  gfx::Vector2d bottom_right_diff =
      tile_rect.bottom_right() - quad->rect.bottom_right();
  tex_coord_rect.Inset(top_left_diff.x() / tex_to_geom_scale_x,
                       top_left_diff.y() / tex_to_geom_scale_y,
                       -bottom_right_diff.x() / tex_to_geom_scale_x,
                       -bottom_right_diff.y() / tex_to_geom_scale_y);

  gfx::RectF clamp_geom_rect(tile_rect);
  gfx::RectF clamp_tex_rect(tex_coord_rect);
  // Clamp texture coordinates to avoid sampling outside the layer
  // by deflating the tile region half a texel or half a texel
  // minus epsilon for one pixel layers. The resulting clamp region
  // is mapped to the unit square by the vertex shader and mapped
  // back to normalized texture coordinates by the fragment shader
  // after being clamped to 0-1 range.
  const float epsilon = 1.0f / 1024.0f;
  float tex_clamp_x = std::min(0.5f, 0.5f * clamp_tex_rect.width() - epsilon);
  float tex_clamp_y = std::min(0.5f, 0.5f * clamp_tex_rect.height() - epsilon);
  float geom_clamp_x = std::min(tex_clamp_x * tex_to_geom_scale_x,
                                0.5f * clamp_geom_rect.width() - epsilon);
  float geom_clamp_y = std::min(tex_clamp_y * tex_to_geom_scale_y,
                                0.5f * clamp_geom_rect.height() - epsilon);
  clamp_geom_rect.Inset(geom_clamp_x, geom_clamp_y, geom_clamp_x, geom_clamp_y);
  clamp_tex_rect.Inset(tex_clamp_x, tex_clamp_y, tex_clamp_x, tex_clamp_y);

  // Map clamping rectangle to unit square.
  float vertex_tex_translate_x = -clamp_geom_rect.x() / clamp_geom_rect.width();
  float vertex_tex_translate_y =
      -clamp_geom_rect.y() / clamp_geom_rect.height();
  float vertex_tex_scale_x = tile_rect.width() / clamp_geom_rect.width();
  float vertex_tex_scale_y = tile_rect.height() / clamp_geom_rect.height();

  // Map to normalized texture coordinates.
  gfx::Size texture_size = quad->texture_size;
  float fragment_tex_translate_x = clamp_tex_rect.x() / texture_size.width();
  float fragment_tex_translate_y = clamp_tex_rect.y() / texture_size.height();
  float fragment_tex_scale_x = clamp_tex_rect.width() / texture_size.width();
  float fragment_tex_scale_y = clamp_tex_rect.height() / texture_size.height();

  gfx::Transform device_transform =
      frame->window_matrix * frame->projection_matrix * quad->quadTransform();
  device_transform.FlattenTo2d();
  if (!device_transform.IsInvertible())
    return;

  gfx::QuadF local_quad = gfx::QuadF(gfx::RectF(tile_rect));
  float edge[24];
  bool use_aa = SetupQuadForAntialiasing(
      device_transform, quad, &local_quad, edge);

  TileProgramUniforms uniforms;
  if (use_aa) {
    if (quad->swizzle_contents)
      TileUniformLocation(GetTileProgramSwizzleAA(), &uniforms);
    else
      TileUniformLocation(GetTileProgramAA(), &uniforms);
  } else {
    if (quad->ShouldDrawWithBlending()) {
      if (quad->swizzle_contents)
        TileUniformLocation(GetTileProgramSwizzle(), &uniforms);
      else
        TileUniformLocation(GetTileProgram(), &uniforms);
    } else {
      if (quad->swizzle_contents)
        TileUniformLocation(GetTileProgramSwizzleOpaque(), &uniforms);
      else
        TileUniformLocation(GetTileProgramOpaque(), &uniforms);
    }
  }

  SetUseProgram(uniforms.program);
  GLC(Context(), Context()->uniform1i(uniforms.sampler_location, 0));
  bool scaled = (tex_to_geom_scale_x != 1.f || tex_to_geom_scale_y != 1.f);
  GLenum filter = (use_aa || scaled ||
                   !quad->quadTransform().IsIdentityOrIntegerTranslation())
                  ? GL_LINEAR
                  : GL_NEAREST;
  ResourceProvider::ScopedSamplerGL quad_resource_lock(
      resource_provider_, quad->resource_id, GL_TEXTURE_2D, filter);

  if (use_aa) {
    GLC(Context(), Context()->uniform3fv(uniforms.edge_location, 8, edge));

    GLC(Context(),
        Context()->uniform4f(uniforms.vertex_tex_transform_location,
                             vertex_tex_translate_x,
                             vertex_tex_translate_y,
                             vertex_tex_scale_x,
                             vertex_tex_scale_y));
    GLC(Context(),
        Context()->uniform4f(uniforms.fragment_tex_transform_location,
                             fragment_tex_translate_x,
                             fragment_tex_translate_y,
                             fragment_tex_scale_x,
                             fragment_tex_scale_y));
  } else {
    // Move fragment shader transform to vertex shader. We can do this while
    // still producing correct results as fragment_tex_transform_location
    // should always be non-negative when tiles are transformed in a way
    // that could result in sampling outside the layer.
    vertex_tex_scale_x *= fragment_tex_scale_x;
    vertex_tex_scale_y *= fragment_tex_scale_y;
    vertex_tex_translate_x *= fragment_tex_scale_x;
    vertex_tex_translate_y *= fragment_tex_scale_y;
    vertex_tex_translate_x += fragment_tex_translate_x;
    vertex_tex_translate_y += fragment_tex_translate_y;

    GLC(Context(),
        Context()->uniform4f(uniforms.vertex_tex_transform_location,
                             vertex_tex_translate_x,
                             vertex_tex_translate_y,
                             vertex_tex_scale_x,
                             vertex_tex_scale_y));
  }

  // Enable blending when the quad properties require it or if we decided
  // to use antialiasing.
  SetBlendEnabled(quad->ShouldDrawWithBlending() || use_aa);

  // Normalize to tile_rect.
  local_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());

  SetShaderOpacity(quad->opacity(), uniforms.alpha_location);
  SetShaderQuadF(local_quad, uniforms.point_location);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  gfx::RectF centered_rect(
      gfx::PointF(-0.5f * tile_rect.width(), -0.5f * tile_rect.height()),
      tile_rect.size());
  DrawQuadGeometry(
      frame, quad->quadTransform(), centered_rect, uniforms.matrix_location);
}

void GLRenderer::DrawYUVVideoQuad(const DrawingFrame* frame,
                                  const YUVVideoDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  const VideoYUVProgram* program = GetVideoYUVProgram();
  DCHECK(program && (program->initialized() || IsContextLost()));

  const VideoLayerImpl::FramePlane& y_plane = quad->y_plane;
  const VideoLayerImpl::FramePlane& u_plane = quad->u_plane;
  const VideoLayerImpl::FramePlane& v_plane = quad->v_plane;

  GLC(Context(), Context()->activeTexture(GL_TEXTURE1));
  ResourceProvider::ScopedSamplerGL y_plane_lock(
      resource_provider_, y_plane.resource_id, GL_TEXTURE_2D, GL_LINEAR);
  GLC(Context(), Context()->activeTexture(GL_TEXTURE2));
  ResourceProvider::ScopedSamplerGL u_plane_lock(
      resource_provider_, u_plane.resource_id, GL_TEXTURE_2D, GL_LINEAR);
  GLC(Context(), Context()->activeTexture(GL_TEXTURE3));
  ResourceProvider::ScopedSamplerGL v_plane_lock(
      resource_provider_, v_plane.resource_id, GL_TEXTURE_2D, GL_LINEAR);

  SetUseProgram(program->program());

  GLC(Context(),
      Context()->uniform2f(program->vertex_shader().tex_scale_location(),
                           quad->tex_scale.width(),
                           quad->tex_scale.height()));
  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().y_texture_location(), 1));
  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().u_texture_location(), 2));
  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().v_texture_location(), 3));

  // These values are magic numbers that are used in the transformation from YUV
  // to RGB color values.  They are taken from the following webpage:
  // http://www.fourcc.org/fccyvrgb.php
  float yuv_to_rgb[9] = {
      1.164f, 1.164f, 1.164f,
      0.0f, -.391f, 2.018f,
      1.596f, -.813f, 0.0f,
  };
  GLC(Context(),
      Context()->uniformMatrix3fv(
          program->fragment_shader().yuv_matrix_location(), 1, 0, yuv_to_rgb));

  // These values map to 16, 128, and 128 respectively, and are computed
  // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
  // They are used in the YUV to RGBA conversion formula:
  //   Y - 16   : Gives 16 values of head and footroom for overshooting
  //   U - 128  : Turns unsigned U into signed U [-128,127]
  //   V - 128  : Turns unsigned V into signed V [-128,127]
  float yuv_adjust[3] = { -0.0625f, -0.5f, -0.5f, };
  GLC(Context(),
      Context()->uniform3fv(
          program->fragment_shader().yuv_adj_location(), 1, yuv_adjust));

  SetShaderOpacity(quad->opacity(),
                   program->fragment_shader().alpha_location());
  DrawQuadGeometry(frame,
                   quad->quadTransform(),
                   quad->rect,
                   program->vertex_shader().matrix_location());

  // Reset active texture back to texture 0.
  GLC(Context(), Context()->activeTexture(GL_TEXTURE0));
}

void GLRenderer::DrawStreamVideoQuad(const DrawingFrame* frame,
                                     const StreamVideoDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  static float gl_matrix[16];

  DCHECK(capabilities_.using_egl_image);

  const VideoStreamTextureProgram* program = GetVideoStreamTextureProgram();
  SetUseProgram(program->program());

  ToGLMatrix(&gl_matrix[0], quad->matrix);
  GLC(Context(),
      Context()->uniformMatrix4fv(
          program->vertex_shader().tex_matrix_location(), 1, false, gl_matrix));

  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_EXTERNAL_OES, quad->texture_id));

  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().sampler_location(), 0));

  SetShaderOpacity(quad->opacity(),
                   program->fragment_shader().alpha_location());
  DrawQuadGeometry(frame,
                   quad->quadTransform(),
                   quad->rect,
                   program->vertex_shader().matrix_location());
}

struct TextureProgramBinding {
  template <class Program>
  void Set(Program* program, WebKit::WebGraphicsContext3D* context) {
    DCHECK(program && (program->initialized() || context->isContextLost()));
    program_id = program->program();
    sampler_location = program->fragment_shader().sampler_location();
    matrix_location = program->vertex_shader().matrix_location();
    alpha_location = program->fragment_shader().alpha_location();
  }
  int program_id;
  int sampler_location;
  int matrix_location;
  int alpha_location;
};

struct TexTransformTextureProgramBinding : TextureProgramBinding {
  template <class Program>
  void Set(Program* program, WebKit::WebGraphicsContext3D* context) {
    TextureProgramBinding::Set(program, context);
    tex_transform_location = program->vertex_shader().tex_transform_location();
    vertex_opacity_location =
        program->vertex_shader().vertex_opacity_location();
  }
  int tex_transform_location;
  int vertex_opacity_location;
};

void GLRenderer::FlushTextureQuadCache() {
  // Check to see if we have anything to draw.
  if (draw_cache_.program_id == 0)
    return;

  // Set the correct blending mode.
  SetBlendEnabled(draw_cache_.needs_blending);

  // Bind the program to the GL state.
  SetUseProgram(draw_cache_.program_id);

  // Bind the correct texture sampler location.
  GLC(Context(), Context()->uniform1i(draw_cache_.sampler_location, 0));

  // Assume the current active textures is 0.
  ResourceProvider::ScopedReadLockGL locked_quad(resource_provider_,
                                                 draw_cache_.resource_id);
  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_2D, locked_quad.texture_id()));

  // set up premultiplied alpha.
  if (!draw_cache_.use_premultiplied_alpha) {
    // As it turns out, the premultiplied alpha blending function (ONE,
    // ONE_MINUS_SRC_ALPHA) will never cause the alpha channel to be set to
    // anything less than 1.0f if it is initialized to that value! Therefore,
    // premultiplied_alpha being false is the first situation we can generally
    // see an alpha channel less than 1.0f coming out of the compositor. This is
    // causing platform differences in some layout tests (see
    // https://bugs.webkit.org/show_bug.cgi?id=82412), so in this situation, use
    // a separate blend function for the alpha channel to avoid modifying it.
    // Don't use colorMask() for this as it has performance implications on some
    // platforms.
    GLC(Context(),
        Context()->blendFuncSeparate(
            GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
  }

  COMPILE_ASSERT(sizeof(Float4) == 4 * sizeof(float), struct_is_densely_packed);
  COMPILE_ASSERT(sizeof(Float16) == 16 * sizeof(float),
                 struct_is_densely_packed);

  // Upload the tranforms for both points and uvs.
  GLC(context_,
      context_->uniformMatrix4fv(
          static_cast<int>(draw_cache_.matrix_location),
          static_cast<int>(draw_cache_.matrix_data.size()),
          false,
          reinterpret_cast<float*>(&draw_cache_.matrix_data.front())));
  GLC(context_,
      context_->uniform4fv(
          static_cast<int>(draw_cache_.uv_xform_location),
          static_cast<int>(draw_cache_.uv_xform_data.size()),
          reinterpret_cast<float*>(&draw_cache_.uv_xform_data.front())));
  GLC(context_,
      context_->uniform1fv(
          static_cast<int>(draw_cache_.vertex_opacity_location),
          static_cast<int>(draw_cache_.vertex_opacity_data.size()),
          static_cast<float*>(&draw_cache_.vertex_opacity_data.front())));

  // Draw the quads!
  GLC(context_,
      context_->drawElements(GL_TRIANGLES,
                             6 * draw_cache_.matrix_data.size(),
                             GL_UNSIGNED_SHORT,
                             0));

  // Clean up after ourselves (reset state set above).
  if (!draw_cache_.use_premultiplied_alpha)
    GLC(context_, context_->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

  // Clear the cache.
  draw_cache_.program_id = 0;
  draw_cache_.uv_xform_data.resize(0);
  draw_cache_.vertex_opacity_data.resize(0);
  draw_cache_.matrix_data.resize(0);
}

void GLRenderer::EnqueueTextureQuad(const DrawingFrame* frame,
                                    const TextureDrawQuad* quad) {
  // Choose the correct texture program binding
  TexTransformTextureProgramBinding binding;
  if (quad->flipped)
    binding.Set(GetTextureProgramFlip(), Context());
  else
    binding.Set(GetTextureProgram(), Context());

  int resource_id = quad->resource_id;

  if (draw_cache_.program_id != binding.program_id ||
      draw_cache_.resource_id != resource_id ||
      draw_cache_.use_premultiplied_alpha != quad->premultiplied_alpha ||
      draw_cache_.needs_blending != quad->ShouldDrawWithBlending() ||
      draw_cache_.matrix_data.size() >= 8) {
    FlushTextureQuadCache();
    draw_cache_.program_id = binding.program_id;
    draw_cache_.resource_id = resource_id;
    draw_cache_.use_premultiplied_alpha = quad->premultiplied_alpha;
    draw_cache_.needs_blending = quad->ShouldDrawWithBlending();

    draw_cache_.uv_xform_location = binding.tex_transform_location;
    draw_cache_.vertex_opacity_location = binding.vertex_opacity_location;
    draw_cache_.matrix_location = binding.matrix_location;
    draw_cache_.sampler_location = binding.sampler_location;
  }

  // Generate the uv-transform
  gfx::PointF uv0 = quad->uv_top_left;
  gfx::PointF uv1 = quad->uv_bottom_right;
  Float4 uv = { { uv0.x(), uv0.y(), uv1.x() - uv0.x(), uv1.y() - uv0.y() } };
  draw_cache_.uv_xform_data.push_back(uv);

  // Generate the vertex opacity
  const float opacity = quad->opacity();
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[0] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[1] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[2] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[3] * opacity);

  // Generate the transform matrix
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, quad->quadTransform(), quad->rect);
  quad_rect_matrix = frame->projection_matrix * quad_rect_matrix;

  Float16 m;
  quad_rect_matrix.matrix().asColMajorf(m.data);
  draw_cache_.matrix_data.push_back(m);
}

void GLRenderer::DrawTextureQuad(const DrawingFrame* frame,
                                 const TextureDrawQuad* quad) {
  TexTransformTextureProgramBinding binding;
  if (quad->flipped)
    binding.Set(GetTextureProgramFlip(), Context());
  else
    binding.Set(GetTextureProgram(), Context());
  SetUseProgram(binding.program_id);
  GLC(Context(), Context()->uniform1i(binding.sampler_location, 0));
  gfx::PointF uv0 = quad->uv_top_left;
  gfx::PointF uv1 = quad->uv_bottom_right;
  GLC(Context(),
      Context()->uniform4f(binding.tex_transform_location,
                           uv0.x(),
                           uv0.y(),
                           uv1.x() - uv0.x(),
                           uv1.y() - uv0.y()));

  GLC(Context(),
      Context()->uniform1fv(
          binding.vertex_opacity_location, 4, quad->vertex_opacity));

  ResourceProvider::ScopedSamplerGL quad_resource_lock(
      resource_provider_, quad->resource_id, GL_TEXTURE_2D, GL_LINEAR);

  if (!quad->premultiplied_alpha) {
    // As it turns out, the premultiplied alpha blending function (ONE,
    // ONE_MINUS_SRC_ALPHA) will never cause the alpha channel to be set to
    // anything less than 1.0f if it is initialized to that value! Therefore,
    // premultiplied_alpha being false is the first situation we can generally
    // see an alpha channel less than 1.0f coming out of the compositor. This is
    // causing platform differences in some layout tests (see
    // https://bugs.webkit.org/show_bug.cgi?id=82412), so in this situation, use
    // a separate blend function for the alpha channel to avoid modifying it.
    // Don't use colorMask() for this as it has performance implications on some
    // platforms.
    GLC(Context(),
        Context()->blendFuncSeparate(
            GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
  }

  DrawQuadGeometry(
      frame, quad->quadTransform(), quad->rect, binding.matrix_location);

  if (!quad->premultiplied_alpha)
    GLC(context_, context_->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
}

void GLRenderer::DrawIOSurfaceQuad(const DrawingFrame* frame,
                                   const IOSurfaceDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  TexTransformTextureProgramBinding binding;
  binding.Set(GetTextureIOSurfaceProgram(), Context());

  SetUseProgram(binding.program_id);
  GLC(Context(), Context()->uniform1i(binding.sampler_location, 0));
  if (quad->orientation == IOSurfaceDrawQuad::FLIPPED) {
    GLC(Context(),
        Context()->uniform4f(binding.tex_transform_location,
                             0,
                             quad->io_surface_size.height(),
                             quad->io_surface_size.width(),
                             quad->io_surface_size.height() * -1.0f));
  } else {
    GLC(Context(),
        Context()->uniform4f(binding.tex_transform_location,
                             0,
                             0,
                             quad->io_surface_size.width(),
                             quad->io_surface_size.height()));
  }

  const float vertex_opacity[] = { quad->opacity(), quad->opacity(),
                                   quad->opacity(), quad->opacity() };
  GLC(Context(),
      Context()->uniform1fv(
          binding.vertex_opacity_location, 4, vertex_opacity));

  GLC(Context(),
      Context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB,
                             quad->io_surface_texture_id));

  DrawQuadGeometry(
      frame, quad->quadTransform(), quad->rect, binding.matrix_location);

  GLC(Context(), Context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB, 0));
}

void GLRenderer::FinishDrawingFrame(DrawingFrame* frame) {
  current_framebuffer_lock_.reset();
  swap_buffer_rect_.Union(gfx::ToEnclosingRect(frame->root_damage_rect));

  GLC(context_, context_->disable(GL_BLEND));
  blend_shadow_ = false;

  if (Settings().compositor_frame_message) {
    CompositorFrame compositor_frame;
    compositor_frame.metadata = client_->MakeCompositorFrameMetadata();
    output_surface_->SendFrameToParentCompositor(&compositor_frame);
  }
}

void GLRenderer::FinishDrawingQuadList() { FlushTextureQuadCache(); }

bool GLRenderer::FlippedFramebuffer() const { return true; }

void GLRenderer::EnsureScissorTestEnabled() {
  if (is_scissor_enabled_)
    return;

  FlushTextureQuadCache();
  GLC(context_, context_->enable(GL_SCISSOR_TEST));
  is_scissor_enabled_ = true;
}

void GLRenderer::EnsureScissorTestDisabled() {
  if (!is_scissor_enabled_)
    return;

  FlushTextureQuadCache();
  GLC(context_, context_->disable(GL_SCISSOR_TEST));
  is_scissor_enabled_ = false;
}

void GLRenderer::ToGLMatrix(float* gl_matrix, const gfx::Transform& transform) {
  transform.matrix().asColMajorf(gl_matrix);
}

void GLRenderer::SetShaderQuadF(const gfx::QuadF& quad, int quad_location) {
  if (quad_location == -1)
    return;

  float point[8];
  point[0] = quad.p1().x();
  point[1] = quad.p1().y();
  point[2] = quad.p2().x();
  point[3] = quad.p2().y();
  point[4] = quad.p3().x();
  point[5] = quad.p3().y();
  point[6] = quad.p4().x();
  point[7] = quad.p4().y();
  GLC(context_, context_->uniform2fv(quad_location, 4, point));
}

void GLRenderer::SetShaderOpacity(float opacity, int alpha_location) {
  if (alpha_location != -1)
    GLC(context_, context_->uniform1f(alpha_location, opacity));
}

void GLRenderer::SetBlendEnabled(bool enabled) {
  if (enabled == blend_shadow_)
    return;

  if (enabled)
    GLC(context_, context_->enable(GL_BLEND));
  else
    GLC(context_, context_->disable(GL_BLEND));
  blend_shadow_ = enabled;
}

void GLRenderer::SetUseProgram(unsigned program) {
  if (program == program_shadow_)
    return;
  GLC(context_, context_->useProgram(program));
  program_shadow_ = program;
}

void GLRenderer::DrawQuadGeometry(const DrawingFrame* frame,
                                  const gfx::Transform& draw_transform,
                                  const gfx::RectF& quad_rect,
                                  int matrix_location) {
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, draw_transform, quad_rect);
  static float gl_matrix[16];
  ToGLMatrix(&gl_matrix[0], frame->projection_matrix * quad_rect_matrix);
  GLC(context_,
      context_->uniformMatrix4fv(matrix_location, 1, false, &gl_matrix[0]));

  GLC(context_, context_->drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));
}

void GLRenderer::CopyTextureToFramebuffer(const DrawingFrame* frame,
                                          int texture_id,
                                          gfx::Rect rect,
                                          const gfx::Transform& draw_matrix) {
  const RenderPassProgram* program = GetRenderPassProgram();

  GLC(Context(), Context()->bindTexture(GL_TEXTURE_2D, texture_id));

  SetUseProgram(program->program());
  GLC(Context(),
      Context()->uniform1i(program->fragment_shader().sampler_location(), 0));
  GLC(Context(),
      Context()->uniform4f(program->vertex_shader().tex_transform_location(),
                           0.0f,
                           0.0f,
                           1.0f,
                           1.0f));
  SetShaderOpacity(1, program->fragment_shader().alpha_location());
  DrawQuadGeometry(
      frame, draw_matrix, rect, program->vertex_shader().matrix_location());
}

void GLRenderer::Finish() {
  TRACE_EVENT0("cc", "GLRenderer::finish");
  context_->finish();
}

bool GLRenderer::SwapBuffers() {
  DCHECK(visible_);
  DCHECK(!is_backbuffer_discarded_);

  TRACE_EVENT0("cc", "GLRenderer::SwapBuffers");
  // We're done! Time to swapbuffers!

  if (capabilities_.using_partial_swap) {
    // If supported, we can save significant bandwidth by only swapping the
    // damaged/scissored region (clamped to the viewport)
    swap_buffer_rect_.Intersect(gfx::Rect(ViewportSize()));
    int flipped_y_pos_of_rect_bottom =
        ViewportHeight() - swap_buffer_rect_.y() - swap_buffer_rect_.height();
    output_surface_->PostSubBuffer(gfx::Rect(swap_buffer_rect_.x(),
                                             flipped_y_pos_of_rect_bottom,
                                             swap_buffer_rect_.width(),
                                             swap_buffer_rect_.height()));
  } else {
    output_surface_->SwapBuffers();
  }

  swap_buffer_rect_ = gfx::Rect();

  // We don't have real fences, so we mark read fences as passed
  // assuming a double-buffered GPU pipeline. A texture can be
  // written to after one full frame has past since it was last read.
  if (last_swap_fence_)
    static_cast<SimpleSwapFence*>(last_swap_fence_.get())->SetHasPassed();
  last_swap_fence_ = resource_provider_->GetReadLockFence();
  resource_provider_->SetReadLockFence(new SimpleSwapFence());

  return true;
}

void GLRenderer::ReceiveCompositorFrameAck(const CompositorFrameAck& ack) {
  onSwapBuffersComplete();
}

void GLRenderer::onSwapBuffersComplete() { client_->OnSwapBuffersComplete(); }

void GLRenderer::onMemoryAllocationChanged(
    WebGraphicsMemoryAllocation allocation) {
  // Just ignore the memory manager when it says to set the limit to zero
  // bytes. This will happen when the memory manager thinks that the renderer
  // is not visible (which the renderer knows better).
  if (allocation.bytesLimitWhenVisible) {
    ManagedMemoryPolicy policy(
        allocation.bytesLimitWhenVisible,
        PriorityCutoff(allocation.priorityCutoffWhenVisible),
        allocation.bytesLimitWhenNotVisible,
        PriorityCutoff(allocation.priorityCutoffWhenNotVisible));

    if (allocation.enforceButDoNotKeepAsPolicy)
      client_->EnforceManagedMemoryPolicy(policy);
    else
      client_->SetManagedMemoryPolicy(policy);
  }

  bool old_discard_backbuffer_when_not_visible =
      discard_backbuffer_when_not_visible_;
  discard_backbuffer_when_not_visible_ = !allocation.suggestHaveBackbuffer;
  EnforceMemoryPolicy();
  if (allocation.enforceButDoNotKeepAsPolicy)
    discard_backbuffer_when_not_visible_ =
        old_discard_backbuffer_when_not_visible;
}

ManagedMemoryPolicy::PriorityCutoff GLRenderer::PriorityCutoff(
    WebKit::WebGraphicsMemoryAllocation::PriorityCutoff priority_cutoff) {
  // This is simple a 1:1 map, the names differ only because the WebKit names
  // should be to match the cc names.
  switch (priority_cutoff) {
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_REQUIRED_ONLY;
    case WebKit::WebGraphicsMemoryAllocation::
        PriorityCutoffAllowVisibleAndNearby:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_NICE_TO_HAVE;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything:
      return ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING;
  }
  NOTREACHED();
  return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
}

void GLRenderer::EnforceMemoryPolicy() {
  if (!visible_) {
    TRACE_EVENT0("cc", "GLRenderer::EnforceMemoryPolicy dropping resources");
    ReleaseRenderPassTextures();
    if (discard_backbuffer_when_not_visible_)
      DiscardBackbuffer();
    resource_provider_->ReleaseCachedData();
    GLC(context_, context_->flush());
  }
}

void GLRenderer::DiscardBackbuffer() {
  if (is_backbuffer_discarded_)
    return;

  output_surface_->DiscardBackbuffer();

  is_backbuffer_discarded_ = true;

  // Damage tracker needs a full reset every time framebuffer is discarded.
  client_->SetFullRootLayerDamage();
}

void GLRenderer::EnsureBackbuffer() {
  if (!is_backbuffer_discarded_)
    return;

  output_surface_->EnsureBackbuffer();
  is_backbuffer_discarded_ = false;
}

void GLRenderer::onContextLost() { client_->DidLoseOutputSurface(); }

void GLRenderer::GetFramebufferPixels(void* pixels, gfx::Rect rect) {
  DCHECK(rect.right() <= ViewportWidth());
  DCHECK(rect.bottom() <= ViewportHeight());

  if (!pixels)
    return;

  MakeContextCurrent();

  bool do_workaround = NeedsIOSurfaceReadbackWorkaround();

  GLuint temporary_texture = 0;
  GLuint temporary_fbo = 0;

  if (do_workaround) {
    // On Mac OS X, calling glReadPixels() against an FBO whose color attachment
    // is an IOSurface-backed texture causes corruption of future glReadPixels()
    // calls, even those on different OpenGL contexts. It is believed that this
    // is the root cause of top crasher
    // http://crbug.com/99393. <rdar://problem/10949687>

    temporary_texture = context_->createTexture();
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, temporary_texture));
    GLC(context_,
        context_->texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLC(context_,
        context_->texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLC(context_,
        context_->texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GLC(context_,
        context_->texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    // Copy the contents of the current (IOSurface-backed) framebuffer into a
    // temporary texture.
    GLC(context_,
        context_->copyTexImage2D(GL_TEXTURE_2D,
                                 0,
                                 GL_RGBA,
                                 0,
                                 0,
                                 ViewportSize().width(),
                                 ViewportSize().height(),
                                 0));
    temporary_fbo = context_->createFramebuffer();
    // Attach this texture to an FBO, and perform the readback from that FBO.
    GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, temporary_fbo));
    GLC(context_,
        context_->framebufferTexture2D(GL_FRAMEBUFFER,
                                       GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D,
                                       temporary_texture,
                                       0));

    DCHECK(context_->checkFramebufferStatus(GL_FRAMEBUFFER) ==
           GL_FRAMEBUFFER_COMPLETE);
  }

  scoped_array<uint8_t> src_pixels(
      new uint8_t[rect.width() * rect.height() * 4]);
  GLC(context_,
      context_->readPixels(rect.x(),
                           ViewportSize().height() - rect.bottom(),
                           rect.width(),
                           rect.height(),
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           src_pixels.get()));

  uint8_t* dest_pixels = static_cast<uint8_t*>(pixels);
  size_t row_bytes = rect.width() * 4;
  int num_rows = rect.height();
  size_t total_bytes = num_rows * row_bytes;
  for (size_t dest_y = 0; dest_y < total_bytes; dest_y += row_bytes) {
    // Flip Y axis.
    size_t src_y = total_bytes - dest_y - row_bytes;
    // Swizzle BGRA -> RGBA.
    for (size_t x = 0; x < row_bytes; x += 4) {
      dest_pixels[dest_y + (x + 0)] = src_pixels.get()[src_y + (x + 2)];
      dest_pixels[dest_y + (x + 1)] = src_pixels.get()[src_y + (x + 1)];
      dest_pixels[dest_y + (x + 2)] = src_pixels.get()[src_y + (x + 0)];
      dest_pixels[dest_y + (x + 3)] = src_pixels.get()[src_y + (x + 3)];
    }
  }

  if (do_workaround) {
    // Clean up.
    GLC(context_, context_->bindFramebuffer(GL_FRAMEBUFFER, 0));
    GLC(context_, context_->bindTexture(GL_TEXTURE_2D, 0));
    GLC(context_, context_->deleteFramebuffer(temporary_fbo));
    GLC(context_, context_->deleteTexture(temporary_texture));
  }

  EnforceMemoryPolicy();
}

bool GLRenderer::GetFramebufferTexture(ScopedResource* texture,
                                       gfx::Rect device_rect) {
  DCHECK(!texture->id() || (texture->size() == device_rect.size() &&
                            texture->format() == GL_RGB));

  if (!texture->id() && !texture->Allocate(device_rect.size(),
                                           GL_RGB,
                                           ResourceProvider::TextureUsageAny))
    return false;

  ResourceProvider::ScopedWriteLockGL lock(resource_provider_, texture->id());
  GLC(context_, context_->bindTexture(GL_TEXTURE_2D, lock.texture_id()));
  GLC(context_,
      context_->copyTexImage2D(GL_TEXTURE_2D,
                               0,
                               texture->format(),
                               device_rect.x(),
                               device_rect.y(),
                               device_rect.width(),
                               device_rect.height(),
                               0));
  return true;
}

bool GLRenderer::UseScopedTexture(DrawingFrame* frame,
                                  const ScopedResource* texture,
                                  gfx::Rect viewport_rect) {
  DCHECK(texture->id());
  frame->current_render_pass = NULL;
  frame->current_texture = texture;

  return BindFramebufferToTexture(frame, texture, viewport_rect);
}

void GLRenderer::BindFramebufferToOutputSurface(DrawingFrame* frame) {
  current_framebuffer_lock_.reset();
  output_surface_->BindFramebuffer();
}

bool GLRenderer::BindFramebufferToTexture(DrawingFrame* frame,
                                          const ScopedResource* texture,
                                          gfx::Rect framebuffer_rect) {
  DCHECK(texture->id());

  GLC(context_,
      context_->bindFramebuffer(GL_FRAMEBUFFER, offscreen_framebuffer_id_));
  current_framebuffer_lock_ =
      make_scoped_ptr(new ResourceProvider::ScopedWriteLockGL(
          resource_provider_, texture->id()));
  unsigned texture_id = current_framebuffer_lock_->texture_id();
  GLC(context_,
      context_->framebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0));

  DCHECK(context_->checkFramebufferStatus(GL_FRAMEBUFFER) ==
         GL_FRAMEBUFFER_COMPLETE || IsContextLost());

  InitializeMatrices(frame, framebuffer_rect, false);
  SetDrawViewportSize(framebuffer_rect.size());

  return true;
}

void GLRenderer::SetScissorTestRect(gfx::Rect scissor_rect) {
  EnsureScissorTestEnabled();

  // Don't unnecessarily ask the context to change the scissor, because it
  // may cause undesired GPU pipeline flushes.
  if (scissor_rect == scissor_rect_)
    return;

  scissor_rect_ = scissor_rect;
  FlushTextureQuadCache();
  GLC(context_,
      context_->scissor(scissor_rect.x(),
                        scissor_rect.y(),
                        scissor_rect.width(),
                        scissor_rect.height()));
}

void GLRenderer::SetDrawViewportSize(gfx::Size viewport_size) {
  GLC(context_,
      context_->viewport(0, 0, viewport_size.width(), viewport_size.height()));
}

bool GLRenderer::MakeContextCurrent() { return context_->makeContextCurrent(); }

bool GLRenderer::InitializeSharedObjects() {
  TRACE_EVENT0("cc", "GLRenderer::InitializeSharedObjects");
  MakeContextCurrent();

  // Create an FBO for doing offscreen rendering.
  GLC(context_, offscreen_framebuffer_id_ = context_->createFramebuffer());

  // We will always need these programs to render, so create the programs
  // eagerly so that the shader compilation can start while we do other work.
  // Other programs are created lazily on first access.
  shared_geometry_ =
      make_scoped_ptr(new GeometryBinding(context_, QuadVertexRect()));
  render_pass_program_ = make_scoped_ptr(new RenderPassProgram(context_));
  tile_program_ = make_scoped_ptr(new TileProgram(context_));
  tile_program_opaque_ = make_scoped_ptr(new TileProgramOpaque(context_));

  GLC(context_, context_->flush());

  return true;
}

const GLRenderer::TileCheckerboardProgram*
GLRenderer::GetTileCheckerboardProgram() {
  if (!tile_checkerboard_program_) {
    tile_checkerboard_program_ =
        make_scoped_ptr(new TileCheckerboardProgram(context_));
  }
  if (!tile_checkerboard_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::checkerboardProgram::initalize");
    tile_checkerboard_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_checkerboard_program_.get();
}

const GLRenderer::DebugBorderProgram* GLRenderer::GetDebugBorderProgram() {
  if (!debug_border_program_)
    debug_border_program_ = make_scoped_ptr(new DebugBorderProgram(context_));
  if (!debug_border_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::debugBorderProgram::initialize");
    debug_border_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return debug_border_program_.get();
}

const GLRenderer::SolidColorProgram* GLRenderer::GetSolidColorProgram() {
  if (!solid_color_program_)
    solid_color_program_ = make_scoped_ptr(new SolidColorProgram(context_));
  if (!solid_color_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::solidColorProgram::initialize");
    solid_color_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return solid_color_program_.get();
}

const GLRenderer::SolidColorProgramAA* GLRenderer::GetSolidColorProgramAA() {
  if (!solid_color_program_aa_) {
    solid_color_program_aa_ =
        make_scoped_ptr(new SolidColorProgramAA(context_));
  }
  if (!solid_color_program_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::solidColorProgramAA::initialize");
    solid_color_program_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return solid_color_program_aa_.get();
}

const GLRenderer::RenderPassProgram* GLRenderer::GetRenderPassProgram() {
  DCHECK(render_pass_program_);
  if (!render_pass_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassProgram::initialize");
    render_pass_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return render_pass_program_.get();
}

const GLRenderer::RenderPassProgramAA* GLRenderer::GetRenderPassProgramAA() {
  if (!render_pass_program_aa_)
    render_pass_program_aa_ =
        make_scoped_ptr(new RenderPassProgramAA(context_));
  if (!render_pass_program_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassProgramAA::initialize");
    render_pass_program_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return render_pass_program_aa_.get();
}

const GLRenderer::RenderPassMaskProgram*
GLRenderer::GetRenderPassMaskProgram() {
  if (!render_pass_mask_program_)
    render_pass_mask_program_ =
        make_scoped_ptr(new RenderPassMaskProgram(context_));
  if (!render_pass_mask_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgram::initialize");
    render_pass_mask_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return render_pass_mask_program_.get();
}

const GLRenderer::RenderPassMaskProgramAA*
GLRenderer::GetRenderPassMaskProgramAA() {
  if (!render_pass_mask_program_aa_)
    render_pass_mask_program_aa_ =
        make_scoped_ptr(new RenderPassMaskProgramAA(context_));
  if (!render_pass_mask_program_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgramAA::initialize");
    render_pass_mask_program_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return render_pass_mask_program_aa_.get();
}

const GLRenderer::TileProgram* GLRenderer::GetTileProgram() {
  DCHECK(tile_program_);
  if (!tile_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgram::initialize");
    tile_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_.get();
}

const GLRenderer::TileProgramOpaque* GLRenderer::GetTileProgramOpaque() {
  DCHECK(tile_program_opaque_);
  if (!tile_program_opaque_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramOpaque::initialize");
    tile_program_opaque_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_opaque_.get();
}

const GLRenderer::TileProgramAA* GLRenderer::GetTileProgramAA() {
  if (!tile_program_aa_)
    tile_program_aa_ = make_scoped_ptr(new TileProgramAA(context_));
  if (!tile_program_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramAA::initialize");
    tile_program_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_aa_.get();
}

const GLRenderer::TileProgramSwizzle* GLRenderer::GetTileProgramSwizzle() {
  if (!tile_program_swizzle_)
    tile_program_swizzle_ = make_scoped_ptr(new TileProgramSwizzle(context_));
  if (!tile_program_swizzle_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzle::initialize");
    tile_program_swizzle_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_swizzle_.get();
}

const GLRenderer::TileProgramSwizzleOpaque*
GLRenderer::GetTileProgramSwizzleOpaque() {
  if (!tile_program_swizzle_opaque_)
    tile_program_swizzle_opaque_ =
        make_scoped_ptr(new TileProgramSwizzleOpaque(context_));
  if (!tile_program_swizzle_opaque_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleOpaque::initialize");
    tile_program_swizzle_opaque_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_swizzle_opaque_.get();
}

const GLRenderer::TileProgramSwizzleAA* GLRenderer::GetTileProgramSwizzleAA() {
  if (!tile_program_swizzle_aa_)
    tile_program_swizzle_aa_ =
        make_scoped_ptr(new TileProgramSwizzleAA(context_));
  if (!tile_program_swizzle_aa_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleAA::initialize");
    tile_program_swizzle_aa_->Initialize(context_, is_using_bind_uniform_);
  }
  return tile_program_swizzle_aa_.get();
}

const GLRenderer::TextureProgram* GLRenderer::GetTextureProgram() {
  if (!texture_program_)
    texture_program_ = make_scoped_ptr(new TextureProgram(context_));
  if (!texture_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureProgram::initialize");
    texture_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return texture_program_.get();
}

const GLRenderer::TextureProgramFlip* GLRenderer::GetTextureProgramFlip() {
  if (!texture_program_flip_)
    texture_program_flip_ = make_scoped_ptr(new TextureProgramFlip(context_));
  if (!texture_program_flip_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureProgramFlip::initialize");
    texture_program_flip_->Initialize(context_, is_using_bind_uniform_);
  }
  return texture_program_flip_.get();
}

const GLRenderer::TextureIOSurfaceProgram*
GLRenderer::GetTextureIOSurfaceProgram() {
  if (!texture_io_surface_program_)
    texture_io_surface_program_ =
        make_scoped_ptr(new TextureIOSurfaceProgram(context_));
  if (!texture_io_surface_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::textureIOSurfaceProgram::initialize");
    texture_io_surface_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return texture_io_surface_program_.get();
}

const GLRenderer::VideoYUVProgram* GLRenderer::GetVideoYUVProgram() {
  if (!video_yuv_program_)
    video_yuv_program_ = make_scoped_ptr(new VideoYUVProgram(context_));
  if (!video_yuv_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::videoYUVProgram::initialize");
    video_yuv_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return video_yuv_program_.get();
}

const GLRenderer::VideoStreamTextureProgram*
GLRenderer::GetVideoStreamTextureProgram() {
  if (!Capabilities().using_egl_image)
    return NULL;
  if (!video_stream_texture_program_)
    video_stream_texture_program_ =
        make_scoped_ptr(new VideoStreamTextureProgram(context_));
  if (!video_stream_texture_program_->initialized()) {
    TRACE_EVENT0("cc", "GLRenderer::streamTextureProgram::initialize");
    video_stream_texture_program_->Initialize(context_, is_using_bind_uniform_);
  }
  return video_stream_texture_program_.get();
}

void GLRenderer::CleanupSharedObjects() {
  MakeContextCurrent();

  shared_geometry_.reset();

  if (tile_program_)
    tile_program_->Cleanup(context_);
  if (tile_program_opaque_)
    tile_program_opaque_->Cleanup(context_);
  if (tile_program_swizzle_)
    tile_program_swizzle_->Cleanup(context_);
  if (tile_program_swizzle_opaque_)
    tile_program_swizzle_opaque_->Cleanup(context_);
  if (tile_program_aa_)
    tile_program_aa_->Cleanup(context_);
  if (tile_program_swizzle_aa_)
    tile_program_swizzle_aa_->Cleanup(context_);
  if (tile_checkerboard_program_)
    tile_checkerboard_program_->Cleanup(context_);

  if (render_pass_mask_program_)
    render_pass_mask_program_->Cleanup(context_);
  if (render_pass_program_)
    render_pass_program_->Cleanup(context_);
  if (render_pass_mask_program_aa_)
    render_pass_mask_program_aa_->Cleanup(context_);
  if (render_pass_program_aa_)
    render_pass_program_aa_->Cleanup(context_);

  if (texture_program_)
    texture_program_->Cleanup(context_);
  if (texture_program_flip_)
    texture_program_flip_->Cleanup(context_);
  if (texture_io_surface_program_)
    texture_io_surface_program_->Cleanup(context_);

  if (video_yuv_program_)
    video_yuv_program_->Cleanup(context_);
  if (video_stream_texture_program_)
    video_stream_texture_program_->Cleanup(context_);

  if (debug_border_program_)
    debug_border_program_->Cleanup(context_);
  if (solid_color_program_)
    solid_color_program_->Cleanup(context_);
  if (solid_color_program_aa_)
    solid_color_program_aa_->Cleanup(context_);

  if (offscreen_framebuffer_id_)
    GLC(context_, context_->deleteFramebuffer(offscreen_framebuffer_id_));

  ReleaseRenderPassTextures();
}

bool GLRenderer::IsContextLost() {
  return (context_->getGraphicsResetStatusARB() != GL_NO_ERROR);
}

}  // namespace cc
