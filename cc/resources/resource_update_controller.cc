// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_update_controller.h"

#include <limits>

#include "base/debug/trace_event.h"
#include "cc/base/thread.h"
#include "cc/output/context_provider.h"
#include "cc/output/texture_copier.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_provider.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"

using WebKit::WebGraphicsContext3D;

namespace {

// Number of partial updates we allow.
const size_t kPartialTextureUpdatesMax = 12;

// Measured in seconds.
const double kTextureUpdateTickRate = 0.004;

// Measured in seconds.
const double kUploaderBusyTickRate = 0.001;

// Number of blocking update intervals to allow.
const size_t kMaxBlockingUpdateIntervals = 4;

skia::RefPtr<SkCanvas> CreateAcceleratedCanvas(
    GrContext* gr_context, gfx::Size canvas_size, unsigned texture_id) {
  GrBackendTextureDesc texture_desc;
  texture_desc.fFlags = kRenderTarget_GrBackendTextureFlag;
  texture_desc.fWidth = canvas_size.width();
  texture_desc.fHeight = canvas_size.height();
  texture_desc.fConfig = kSkia8888_GrPixelConfig;
  texture_desc.fTextureHandle = texture_id;
  skia::RefPtr<GrTexture> target =
      skia::AdoptRef(gr_context->wrapBackendTexture(texture_desc));
  skia::RefPtr<SkDevice> device =
      skia::AdoptRef(new SkGpuDevice(gr_context, target.get()));
  return skia::AdoptRef(new SkCanvas(device.get()));
}

}  // namespace

namespace cc {

size_t ResourceUpdateController::MaxPartialTextureUpdates() {
  return kPartialTextureUpdatesMax;
}

size_t ResourceUpdateController::MaxFullUpdatesPerTick(
    ResourceProvider* resource_provider) {
  double textures_per_second = resource_provider->EstimatedUploadsPerSecond();
  size_t textures_per_tick =
      floor(kTextureUpdateTickRate * textures_per_second);
  return textures_per_tick ? textures_per_tick : 1;
}

ResourceUpdateController::ResourceUpdateController(
    ResourceUpdateControllerClient* client,
    Thread* thread,
    scoped_ptr<ResourceUpdateQueue> queue,
    ResourceProvider* resource_provider)
    : client_(client),
      queue_(queue.Pass()),
      resource_provider_(resource_provider),
      texture_updates_per_tick_(MaxFullUpdatesPerTick(resource_provider)),
      first_update_attempt_(true),
      thread_(thread),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      task_posted_(false) {}

ResourceUpdateController::~ResourceUpdateController() {}

void ResourceUpdateController::PerformMoreUpdates(
    base::TimeTicks time_limit) {
  time_limit_ = time_limit;

  // Update already in progress.
  if (task_posted_)
    return;

  // Call UpdateMoreTexturesNow() directly unless it's the first update
  // attempt. This ensures that we empty the update queue in a finite
  // amount of time.
  if (!first_update_attempt_)
    UpdateMoreTexturesNow();

  // Post a 0-delay task when no updates were left. When it runs,
  // ReadyToFinalizeTextureUpdates() will be called.
  if (!UpdateMoreTexturesIfEnoughTimeRemaining()) {
    task_posted_ = true;
    thread_->PostTask(
        base::Bind(&ResourceUpdateController::OnTimerFired,
                   weak_factory_.GetWeakPtr()));
  }

  first_update_attempt_ = false;
}

void ResourceUpdateController::DiscardUploadsToEvictedResources() {
  queue_->ClearUploadsToEvictedResources();
}

void ResourceUpdateController::UpdateTexture(ResourceUpdate update) {
  if (update.picture) {
    PrioritizedResource* texture = update.texture;
    gfx::Rect picture_rect = update.content_rect;
    gfx::Rect source_rect = update.source_rect;
    gfx::Vector2d dest_offset = update.dest_offset;

    texture->AcquireBackingTexture(resource_provider_);
    DCHECK(texture->have_backing_texture());

    DCHECK_EQ(resource_provider_->GetResourceType(texture->resource_id()),
              ResourceProvider::GLTexture);

    cc::ContextProvider* offscreen_contexts =
        resource_provider_->offscreen_context_provider();

    ResourceProvider::ScopedWriteLockGL lock(
        resource_provider_, texture->resource_id());

    // Flush the compositor context to ensure that textures there are available
    // in the shared context.  Do this after locking/creating the compositor
    // texture.
    resource_provider_->Flush();

    // Make sure skia uses the correct GL context.
    offscreen_contexts->Context3d()->makeContextCurrent();

    // Create an accelerated canvas to draw on.
    skia::RefPtr<SkCanvas> canvas = CreateAcceleratedCanvas(
        offscreen_contexts->GrContext(), texture->size(), lock.texture_id());

    // The compositor expects the textures to be upside-down so it can flip
    // the final composited image. Ganesh renders the image upright so we
    // need to do a y-flip.
    canvas->translate(0.0, texture->size().height());
    canvas->scale(1.0, -1.0);
    // Clip to the destination on the texture that must be updated.
    canvas->clipRect(SkRect::MakeXYWH(dest_offset.x(),
                                      dest_offset.y(),
                                      source_rect.width(),
                                      source_rect.height()));
    // Translate the origin of picture_rect to dest_offset.
    // Note that dest_offset is defined relative to source_rect.
    canvas->translate(
        picture_rect.x() - source_rect.x() + dest_offset.x(),
        picture_rect.y() - source_rect.y() + dest_offset.y());
    canvas->drawPicture(*update.picture);

    // Flush skia context so that all the rendered stuff appears on the
    // texture.
    offscreen_contexts->GrContext()->flush();

    // Flush the GL context so rendering results from this context are
    // visible in the compositor's context.
    offscreen_contexts->Context3d()->flush();

    // Use the compositor's GL context again.
    resource_provider_->GraphicsContext3D()->makeContextCurrent();
  }

  if (update.bitmap) {
    update.bitmap->lockPixels();
    update.texture->SetPixels(
        resource_provider_,
        static_cast<const uint8_t*>(update.bitmap->getPixels()),
        update.content_rect,
        update.source_rect,
        update.dest_offset);
    update.bitmap->unlockPixels();
  }
}

void ResourceUpdateController::Finalize() {
  while (queue_->FullUploadSize())
    UpdateTexture(queue_->TakeFirstFullUpload());

  while (queue_->PartialUploadSize())
    UpdateTexture(queue_->TakeFirstPartialUpload());

  resource_provider_->FlushUploads();

  if (queue_->CopySize()) {
    TextureCopier* copier = resource_provider_->texture_copier();
    while (queue_->CopySize())
      copier->CopyTexture(queue_->TakeFirstCopy());

    // If we've performed any texture copies, we need to insert a flush
    // here into the compositor context before letting the main thread
    // proceed as it may make draw calls to the source texture of one of
    // our copy operations.
    copier->Flush();
  }
}

void ResourceUpdateController::OnTimerFired() {
  task_posted_ = false;
  if (!UpdateMoreTexturesIfEnoughTimeRemaining())
    client_->ReadyToFinalizeTextureUpdates();
}

base::TimeTicks ResourceUpdateController::Now() const {
  return base::TimeTicks::Now();
}

base::TimeDelta ResourceUpdateController::UpdateMoreTexturesTime() const {
  return base::TimeDelta::FromMilliseconds(kTextureUpdateTickRate * 1000);
}

size_t ResourceUpdateController::UpdateMoreTexturesSize() const {
  return texture_updates_per_tick_;
}

size_t ResourceUpdateController::MaxBlockingUpdates() const {
  return UpdateMoreTexturesSize() * kMaxBlockingUpdateIntervals;
}

base::TimeDelta ResourceUpdateController::PendingUpdateTime() const {
  base::TimeDelta update_one_resource_time =
      UpdateMoreTexturesTime() / UpdateMoreTexturesSize();
  return update_one_resource_time * resource_provider_->NumBlockingUploads();
}

bool ResourceUpdateController::UpdateMoreTexturesIfEnoughTimeRemaining() {
  while (resource_provider_->NumBlockingUploads() < MaxBlockingUpdates()) {
    if (!queue_->FullUploadSize())
      return false;

    if (!time_limit_.is_null()) {
      // Estimated completion time of all pending updates.
      base::TimeTicks completion_time = Now() + PendingUpdateTime();

      // Time remaining based on current completion estimate.
      base::TimeDelta time_remaining = time_limit_ - completion_time;

      if (time_remaining < UpdateMoreTexturesTime())
        return true;
    }

    UpdateMoreTexturesNow();
  }

  task_posted_ = true;
  thread_->PostDelayedTask(
      base::Bind(&ResourceUpdateController::OnTimerFired,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUploaderBusyTickRate * 1000));
  return true;
}

void ResourceUpdateController::UpdateMoreTexturesNow() {
  size_t uploads = std::min(
      queue_->FullUploadSize(), UpdateMoreTexturesSize());

  if (!uploads)
    return;

  while (queue_->FullUploadSize() && uploads--)
    UpdateTexture(queue_->TakeFirstFullUpload());

  resource_provider_->FlushUploads();
}

}  // namespace cc
