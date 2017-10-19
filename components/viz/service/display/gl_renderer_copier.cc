// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer_copier.h"

#include <algorithm>
#include <cstring>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/service/display/texture_mailbox_deleter.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// Syntactic sugar to DCHECK that two sizes are equal.
#define DCHECK_SIZE_EQ(a, b)                                \
  DCHECK((a) == (b)) << #a " != " #b ": " << (a).ToString() \
                     << " != " << (b).ToString()

namespace viz {

using ResultFormat = CopyOutputRequest::ResultFormat;

namespace {

constexpr int kRGBABytesPerPixel = 4;

// Returns the source property of the |request|, if it is set. Otherwise,
// returns an empty token. This is needed because CopyOutputRequest will crash
// if source() is called when !has_source().
base::UnguessableToken SourceOf(const CopyOutputRequest& request) {
  return request.has_source() ? request.source() : base::UnguessableToken();
}

}  // namespace

GLRendererCopier::GLRendererCopier(
    scoped_refptr<ContextProvider> context_provider,
    TextureMailboxDeleter* texture_mailbox_deleter)
    : context_provider_(std::move(context_provider)),
      texture_mailbox_deleter_(texture_mailbox_deleter),
      helper_(context_provider_->ContextGL(),
              context_provider_->ContextSupport()) {}

GLRendererCopier::~GLRendererCopier() {
  for (auto& entry : cache_)
    FreeCachedResources(&entry.second);
}

void GLRendererCopier::CopyFromTextureOrFramebuffer(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& copy_rect,
    GLenum internal_format,
    GLuint framebuffer_texture,
    const gfx::Size& framebuffer_texture_size,
    const gfx::ColorSpace& color_space) {
  DCHECK(request->has_area());
  // Assumption: The |copy_rect|, although in the framebuffer's coordinate
  // space, should have the same size as the copy request's source area.
  DCHECK_SIZE_EQ(request->area().size(), copy_rect.size());

  // Compute the rect of the pixels in the copy output result's coordinate
  // space that are affected by the requested copy area. If there will be zero
  // pixels of output or the scaling ratio was not reasonable, do not proceed.
  const gfx::Rect result_rect =
      request->is_scaled()
          ? copy_output::ComputeResultRect(
                request->area(), request->scale_from(), request->scale_to())
          : request->area();
  if (result_rect.IsEmpty())
    return;

  // Execute the cheapest workflow that satisfies the copy request.
  switch (request->result_format()) {
    case ResultFormat::RGBA_BITMAP: {
      if (request->is_scaled()) {
        const GLuint result_texture = RenderResultTexture(
            *request, copy_rect, internal_format, framebuffer_texture,
            framebuffer_texture_size, result_rect);
        const base::UnguessableToken& request_source = SourceOf(*request);
        StartReadbackFromTexture(std::move(request), result_texture,
                                 gfx::Rect(result_rect.size()), result_rect,
                                 color_space);
        CacheObjectOrDelete(request_source, CacheEntry::kResultTexture,
                            result_texture);
      } else {
        StartReadbackFromFramebuffer(std::move(request), copy_rect, result_rect,
                                     color_space);
      }
      break;
    }

    case ResultFormat::RGBA_TEXTURE: {
      const GLuint result_texture = RenderResultTexture(
          *request, copy_rect, internal_format, framebuffer_texture,
          framebuffer_texture_size, result_rect);
      SendTextureResult(std::move(request), result_texture, result_rect,
                        color_space);
      break;
    }
  }
}

void GLRendererCopier::FreeUnusedCachedResources() {
  ++purge_counter_;

  // Purge all cache entries that should no longer be kept alive, freeing any
  // resources they held.
  const auto IsTooOld = [this](const decltype(cache_)::value_type& entry) {
    return static_cast<int32_t>(purge_counter_ -
                                entry.second.purge_count_at_last_use) >=
           kKeepalivePeriod;
  };
  for (auto& entry : cache_) {
    if (IsTooOld(entry))
      FreeCachedResources(&entry.second);
  }
  base::EraseIf(cache_, IsTooOld);
}

GLuint GLRendererCopier::RenderResultTexture(
    const CopyOutputRequest& request,
    const gfx::Rect& framebuffer_copy_rect,
    GLenum internal_format,
    GLuint framebuffer_texture,
    const gfx::Size& framebuffer_texture_size,
    const gfx::Rect& result_rect) {
  auto* const gl = context_provider_->ContextGL();

  // Determine the source texture: This is either the one attached to the
  // framebuffer, or a copy made from the framebuffer. Its format will be the
  // same as |internal_format|.
  //
  // TODO(crbug/767221): All of this (including some texture copies) wouldn't be
  // necessary if we could query whether the currently-bound framebuffer has a
  // texture attached to it, and just source from that texture directly (i.e.,
  // using glGetFramebufferAttachmentParameteriv() and
  // glGetTexLevelParameteriv(GL_TEXTURE_WIDTH/HEIGHT)).
  GLuint source_texture;
  gfx::Size source_texture_size;
  gfx::Rect source_copy_rect;
  if (framebuffer_texture != 0) {
    source_texture = framebuffer_texture;
    source_texture_size = framebuffer_texture_size;
    source_copy_rect = framebuffer_copy_rect;
  } else {
    // Optimization: If the texture copy completely satsifies the request, just
    // return it as the result texture. The request must not include scaling nor
    // a texture mailbox to use for delivering results. The texture format must
    // also be GL_RGBA, as described by CopyOutputResult::Format::RGBA_TEXTURE.
    const int purpose =
        (!request.is_scaled() && !request.has_texture_mailbox() &&
         internal_format == GL_RGBA)
            ? CacheEntry::kResultTexture
            : CacheEntry::kFramebufferCopyTexture;
    source_texture = TakeCachedObjectOrCreate(SourceOf(request), purpose);
    gl->BindTexture(GL_TEXTURE_2D, source_texture);
    gl->CopyTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                       framebuffer_copy_rect.x(), framebuffer_copy_rect.y(),
                       framebuffer_copy_rect.width(),
                       framebuffer_copy_rect.height(), 0);
    if (purpose == CacheEntry::kResultTexture)
      return source_texture;
    source_texture_size = framebuffer_copy_rect.size();
    source_copy_rect = gfx::Rect(framebuffer_copy_rect.size());
  }

  // Determine the result texture: If the copy request provided a valid one, use
  // it instead of one owned by GLRendererCopier.
  GLuint result_texture = 0;
  if (request.has_texture_mailbox()) {
    const TextureMailbox& tm = request.texture_mailbox();
    if (!tm.mailbox().IsZero() && tm.target() == GL_TEXTURE_2D) {
      if (tm.sync_token().HasData())
        gl->WaitSyncTokenCHROMIUM(tm.sync_token().GetConstData());
      result_texture =
          gl->CreateAndConsumeTextureCHROMIUM(GL_TEXTURE_2D, tm.mailbox().name);
    }
  }
  if (result_texture == 0) {
    result_texture =
        TakeCachedObjectOrCreate(SourceOf(request), CacheEntry::kResultTexture);
  }
  gl->BindTexture(GL_TEXTURE_2D, result_texture);
  gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, result_rect.width(),
                 result_rect.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  // Populate the result texture with a scaled/exact copy.
  if (request.is_scaled()) {
    const gfx::Rect scaled_copy_rect = copy_output::ComputeResultRect(
        source_copy_rect, request.scale_from(), request.scale_to());
    DCHECK_SIZE_EQ(scaled_copy_rect.size(), result_rect.size());
    std::unique_ptr<GLHelper::ScalerInterface> scaler =
        TakeCachedScalerOrCreate(request);
    scaler->Scale(source_texture, source_texture_size, gfx::Vector2dF(),
                  result_texture, scaled_copy_rect);
    CacheScalerOrDelete(SourceOf(request), std::move(scaler));
  } else {
    DCHECK_SIZE_EQ(source_copy_rect.size(), result_rect.size());
    gl->CopySubTextureCHROMIUM(
        source_texture, 0 /* source_level */, GL_TEXTURE_2D, result_texture,
        0 /* dest_level */, 0 /* xoffset */, 0 /* yoffset */,
        source_copy_rect.x(), source_copy_rect.y(), source_copy_rect.width(),
        source_copy_rect.height(), false, false, false);
  }

  // If |source_texture| was a copy, maybe cache it for future requests.
  if (framebuffer_texture == 0) {
    CacheObjectOrDelete(SourceOf(request), CacheEntry::kFramebufferCopyTexture,
                        source_texture);
  }

  return result_texture;
}

void GLRendererCopier::StartReadbackFromTexture(
    std::unique_ptr<CopyOutputRequest> request,
    GLuint source_texture,
    const gfx::Rect& copy_rect,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space) {
  // Bind |source_texture| to a framebuffer, and then start readback from that.
  const GLuint framebuffer = TakeCachedObjectOrCreate(
      SourceOf(*request), CacheEntry::kReadbackFramebuffer);
  auto* const gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           source_texture, 0);
  const base::UnguessableToken& request_source = SourceOf(*request);
  StartReadbackFromFramebuffer(std::move(request), copy_rect, result_rect,
                               color_space);
  CacheObjectOrDelete(request_source, CacheEntry::kReadbackFramebuffer,
                      framebuffer);
}

namespace {

// Manages the execution of one asynchronous framebuffer read-back and contains
// all the relevant state needed to complete a copy request. The constructor
// initiates the operation, and then at some later point either: 1) the Finish()
// method is invoked; or 2) the instance will be destroyed (cancelled) because
// the GL context is going away. Either way, the GL objects created for this
// workflow are properly cleaned-up.
//
// Motivation: In case #2, it's possible GLRendererCopier will have been
// destroyed before Finish(). However, since there are no dependencies on
// GLRendererCopier to finish the copy request, there's no reason to mess around
// with a complex WeakPtr-to-GLRendererCopier scheme.
class ReadPixelsWorkflow {
 public:
  // Saves all revelant state and initiates the GL asynchronous read-pixels
  // workflow.
  ReadPixelsWorkflow(std::unique_ptr<CopyOutputRequest> copy_request,
                     const gfx::Rect& copy_rect,
                     const gfx::Rect& result_rect,
                     scoped_refptr<ContextProvider> context_provider)
      : copy_request_(std::move(copy_request)),
        result_rect_(result_rect),
        context_provider_(std::move(context_provider)) {
    auto* const gl = context_provider_->ContextGL();

    // Create a buffer for the pixel transfer.
    gl->GenBuffers(1, &transfer_buffer_);
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    gl->BufferData(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM,
                   kRGBABytesPerPixel * result_rect.size().GetArea(), nullptr,
                   GL_STREAM_READ);

    // Execute an asynchronous read-pixels operation, with a query that triggers
    // when Finish() should be run.
    gl->GenQueriesEXT(1, &query_);
    gl->BeginQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, query_);
    gl->ReadPixels(copy_rect.x(), copy_rect.y(), copy_rect.width(),
                   copy_rect.height(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl->EndQueryEXT(GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM);
    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, 0);
  }

  // The destructor is called when the callback that owns this instance is
  // destroyed. That will happen either with or without a call to Finish(),
  // but either way everything will be clean-up appropriately.
  ~ReadPixelsWorkflow() {
    auto* const gl = context_provider_->ContextGL();
    gl->DeleteQueriesEXT(1, &query_);
    gl->DeleteBuffers(1, &transfer_buffer_);
  }

  GLuint query() const { return query_; }

  // Callback for the asynchronous glReadPixels(). The pixels are read from the
  // transfer buffer, and a CopyOutputResult is sent to the requestor.
  void Finish() {
    auto* const gl = context_provider_->ContextGL();

    gl->BindBuffer(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, transfer_buffer_);
    const uint8_t* pixels = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
        GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM, GL_READ_ONLY));
    if (!pixels) {
      // CopyOutputRequest will auto-send an empty result when its destructor
      // is run from ~ReadPixelsWorkflow().
      return;
    }

    // Create the result bitmap, making sure to flip the image in the Y
    // dimension.
    //
    // TODO(crbug/770422): Performance optimization to use GL_BGRA_EXT to avoid
    // extra downstream CPU use for GL_RGBA→SkBitmap N32 byte order swizzling.
    // TODO(crbug/758057): Plumb-through color space into the output bitmap.
    SkBitmap result_bitmap;
    result_bitmap.allocPixels(
        SkImageInfo::Make(result_rect_.width(), result_rect_.height(),
                          kRGBA_8888_SkColorType, kPremul_SkAlphaType));
    const int bytes_per_row = result_rect_.width() * kRGBABytesPerPixel;
    for (int y = 0; y < result_rect_.height(); ++y) {
      const int flipped_y = (result_rect_.height() - y - 1);
      const uint8_t* const src_row = pixels + flipped_y * bytes_per_row;
      void* const dest_row = result_bitmap.getAddr(0, y);
      memcpy(dest_row, src_row, bytes_per_row);
    }
    gl->UnmapBufferCHROMIUM(GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM);

    copy_request_->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
        result_rect_, result_bitmap));

    // |transfer_buffer_| and |query_| will be deleted soon by the destructor.
  }

 private:
  const std::unique_ptr<CopyOutputRequest> copy_request_;
  const gfx::Rect result_rect_;
  const scoped_refptr<ContextProvider> context_provider_;
  GLuint transfer_buffer_ = 0;
  GLuint query_ = 0;
};

}  // namespace

void GLRendererCopier::StartReadbackFromFramebuffer(
    std::unique_ptr<CopyOutputRequest> request,
    const gfx::Rect& copy_rect,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space) {
  DCHECK_NE(request->result_format(), ResultFormat::RGBA_TEXTURE);
  DCHECK_SIZE_EQ(copy_rect.size(), result_rect.size());

  auto workflow = std::make_unique<ReadPixelsWorkflow>(
      std::move(request), copy_rect, result_rect, context_provider_);
  const GLuint query = workflow->query();
  context_provider_->ContextSupport()->SignalQuery(
      query, base::Bind(&ReadPixelsWorkflow::Finish, base::Passed(&workflow)));
}

void GLRendererCopier::SendTextureResult(
    std::unique_ptr<CopyOutputRequest> request,
    GLuint result_texture,
    const gfx::Rect& result_rect,
    const gfx::ColorSpace& color_space) {
  DCHECK_EQ(request->result_format(), ResultFormat::RGBA_TEXTURE);

  auto* const gl = context_provider_->ContextGL();

  // Package the |result_texture| into a TextureMailbox with the required
  // synchronization mechanisms. This lets the requestor ensure operations
  // within its own GL context will be using the texture at a point in time
  // after the texture has been rendered (via GLRendererCopier's GL context).
  gpu::Mailbox mailbox;
  if (request->has_texture_mailbox()) {
    mailbox = request->texture_mailbox().mailbox();
  } else {
    gl->GenMailboxCHROMIUM(mailbox.name);
    gl->ProduceTextureDirectCHROMIUM(result_texture, GL_TEXTURE_2D,
                                     mailbox.name);
  }
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  TextureMailbox texture_mailbox(mailbox, sync_token, GL_TEXTURE_2D);
  texture_mailbox.set_color_space(color_space);

  // Create a |release_callback| appropriate to the situation: If the
  // |result_texture| was provided in the TextureMailbof of the copy request,
  // create a no-op release callback because the requestor owns the texture.
  // Otherwise, create a callback that deletes what was created in this GL
  // context.
  std::unique_ptr<SingleReleaseCallback> release_callback;
  if (request->has_texture_mailbox()) {
    gl->DeleteTextures(1, &result_texture);
    // TODO(crbug/754872): This non-null release callback wart is going away
    // soon, as copy requestors won't need pool/manage textures anymore.
    release_callback = SingleReleaseCallback::Create(
        base::Bind([](const gpu::SyncToken&, bool) {}));
  } else {
    // Note: There's no need to try to pool/re-use the result texture from here,
    // since only clients that are trying to re-invent video capture would see
    // any significant performance benefit. Instead, such clients should use the
    // video capture services provided by VIZ.
    release_callback = texture_mailbox_deleter_->GetReleaseCallback(
        context_provider_, result_texture);
  }

  request->SendResult(std::make_unique<CopyOutputTextureResult>(
      result_rect, texture_mailbox, std::move(release_callback)));
}

GLuint GLRendererCopier::TakeCachedObjectOrCreate(
    const base::UnguessableToken& for_source,
    int which) {
  GLuint object_name = 0;

  // If the object can be found in the cache, take it and return it.
  if (!for_source.is_empty()) {
    GLuint& cached_object_name = cache_[for_source].object_names[which];
    if (cached_object_name != 0) {
      object_name = cached_object_name;
      cached_object_name = 0;
      return object_name;
    }
  }

  // Generate a new one.
  auto* const gl = context_provider_->ContextGL();
  if (which == CacheEntry::kReadbackFramebuffer) {
    gl->GenFramebuffers(1, &object_name);
  } else {
    gl->GenTextures(1, &object_name);
    gl->BindTexture(GL_TEXTURE_2D, object_name);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return object_name;
}

void GLRendererCopier::CacheObjectOrDelete(
    const base::UnguessableToken& for_source,
    int which,
    GLuint object_name) {
  // Do not cache objects for copy requests without a common source.
  if (for_source.is_empty()) {
    auto* const gl = context_provider_->ContextGL();
    if (which == CacheEntry::kReadbackFramebuffer)
      gl->DeleteFramebuffers(1, &object_name);
    else
      gl->DeleteTextures(1, &object_name);
    return;
  }

  CacheEntry& entry = cache_[for_source];
  DCHECK_EQ(entry.object_names[which], 0u);
  entry.object_names[which] = object_name;
  entry.purge_count_at_last_use = purge_counter_;
}

std::unique_ptr<GLHelper::ScalerInterface>
GLRendererCopier::TakeCachedScalerOrCreate(
    const CopyOutputRequest& for_request) {
  // If an identically-configured scaler can be found in the cache, take it and
  // return it. If a differently-configured scaler was found, delete it.
  if (for_request.has_source()) {
    std::unique_ptr<GLHelper::ScalerInterface>& cached_scaler =
        cache_[for_request.source()].scaler;
    if (cached_scaler) {
      if (cached_scaler->IsSameScaleRatio(for_request.scale_from(),
                                          for_request.scale_to())) {
        return std::move(cached_scaler);
      } else {
        cached_scaler.reset();
      }
    }
  }

  // At this point, a new instance must be created. For downscaling, use the
  // GOOD quality setting (appropriate for thumbnailing); and, for upscaling,
  // use the BEST quality.
  const bool is_downscale_in_both_dimensions =
      for_request.scale_to().x() < for_request.scale_from().x() &&
      for_request.scale_to().y() < for_request.scale_from().y();
  const GLHelper::ScalerQuality quality = is_downscale_in_both_dimensions
                                              ? GLHelper::SCALER_QUALITY_GOOD
                                              : GLHelper::SCALER_QUALITY_BEST;
  return helper_.CreateScaler(quality, for_request.scale_from(),
                              for_request.scale_to(), false, false);
}

void GLRendererCopier::CacheScalerOrDelete(
    const base::UnguessableToken& for_source,
    std::unique_ptr<GLHelper::ScalerInterface> scaler) {
  // If the request has a source, cache |scaler| for the next copy request.
  // Otherwise, |scaler| will be auto-deleted on out-of-scope.
  if (!for_source.is_empty()) {
    CacheEntry& entry = cache_[for_source];
    entry.scaler = std::move(scaler);
    entry.purge_count_at_last_use = purge_counter_;
  }
}

void GLRendererCopier::FreeCachedResources(CacheEntry* entry) {
  auto* const gl = context_provider_->ContextGL();
  GLuint* name = &(entry->object_names[CacheEntry::kFramebufferCopyTexture]);
  if (*name != 0)
    gl->DeleteTextures(1, name);
  name = &(entry->object_names[CacheEntry::kResultTexture]);
  if (*name != 0)
    gl->DeleteTextures(1, name);
  name = &(entry->object_names[CacheEntry::kReadbackFramebuffer]);
  if (*name != 0)
    gl->DeleteFramebuffers(1, name);
  entry->object_names.fill(0);
  entry->scaler.reset();
}

GLRendererCopier::CacheEntry::CacheEntry() = default;
GLRendererCopier::CacheEntry::CacheEntry(CacheEntry&&) = default;
GLRendererCopier::CacheEntry& GLRendererCopier::CacheEntry::operator=(
    CacheEntry&&) = default;

GLRendererCopier::CacheEntry::~CacheEntry() {
  // Ensure all resources were freed by this point. Resources aren't explicity
  // freed here, in the destructor, because some require access to the GL
  // context. See FreeCachedResources().
  DCHECK(std::find_if(object_names.begin(), object_names.end(),
                      [](GLuint x) { return x != 0; }) == object_names.end());
  DCHECK(!scaler);
}

}  // namespace viz
