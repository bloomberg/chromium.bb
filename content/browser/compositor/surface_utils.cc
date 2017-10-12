// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_utils.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_main_loop.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"  // nogncheck
#endif

namespace {

#if !defined(OS_ANDROID) || defined(USE_AURA)
void CopyFromCompositingSurfaceFinished(
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    std::unique_ptr<SkBitmap> bitmap,
    bool result) {
  gpu::SyncToken sync_token;
  if (result) {
    viz::GLHelper* gl_helper =
        content::ImageTransportFactory::GetInstance()->GetGLHelper();
    if (gl_helper)
      gl_helper->GenerateSyncToken(&sync_token);
  }
  const bool lost_resource = !sync_token.HasData();
  release_callback->Run(sync_token, lost_resource);

  callback.Run(*bitmap,
               result ? content::READBACK_SUCCESS : content::READBACK_FAILED);
}
#endif

// TODO(wjmaclean): There is significant overlap between
// PrepareTextureCopyOutputResult and CopyFromCompositingSurfaceFinished in
// this file, and the versions in RenderWidgetHostViewAndroid. They should
// be merged. See https://crbug.com/582955
void PrepareTextureCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
#if defined(OS_ANDROID) && !defined(USE_AURA)
  // TODO(wjmaclean): See if there's an equivalent pathway for Android and
  // implement it here.
  callback.Run(SkBitmap(), content::READBACK_FAILED);
#else
  base::ScopedClosureRunner scoped_callback_runner(
      base::BindOnce(callback, SkBitmap(), content::READBACK_FAILED));

  // TODO(siva.gunturi): We should be able to validate the format here using
  // GLHelper::IsReadbackConfigSupported before we processs the result.
  // See crbug.com/415682 and crbug.com/415131.
  std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
  if (!bitmap->tryAllocPixels(SkImageInfo::Make(
          dst_size_in_pixel.width(), dst_size_in_pixel.height(), color_type,
          kOpaque_SkAlphaType))) {
    scoped_callback_runner.ReplaceClosure(base::BindOnce(
        callback, SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE));
    return;
  }

  content::ImageTransportFactory* factory =
      content::ImageTransportFactory::GetInstance();
  viz::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  uint8_t* pixels = static_cast<uint8_t*>(bitmap->getPixels());

  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  if (auto* mailbox = result->GetTextureMailbox()) {
    texture_mailbox = *mailbox;
    release_callback = result->TakeTextureOwnership();
  }
  if (!texture_mailbox.IsTexture())
    return;

  ignore_result(scoped_callback_runner.Release());

  gl_helper->CropScaleReadbackAndCleanMailbox(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(), result->size(),
      dst_size_in_pixel, pixels, color_type,
      base::Bind(&CopyFromCompositingSurfaceFinished, callback,
                 base::Passed(&release_callback), base::Passed(&bitmap)),
      viz::GLHelper::SCALER_QUALITY_GOOD);
#endif
}

void PrepareBitmapCopyOutputResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType preferred_color_type,
    const content::ReadbackRequestCallback& callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
  SkColorType color_type = preferred_color_type;
  if (color_type != kN32_SkColorType && color_type != kAlpha_8_SkColorType) {
    // Switch back to default colortype if format not supported.
    color_type = kN32_SkColorType;
  }
  const SkBitmap source = result->AsSkBitmap();
  if (!source.readyToDraw()) {
    callback.Run(source, content::READBACK_FAILED);
    return;
  }
  SkBitmap scaled_bitmap;
  if (source.width() != dst_size_in_pixel.width() ||
      source.height() != dst_size_in_pixel.height()) {
    // TODO(miu): Delete this logic here and use the new
    // CopyOutputRequest::SetScaleRatio() API. http://crbug.com/760348
    scaled_bitmap = skia::ImageOperations::Resize(
        source, skia::ImageOperations::RESIZE_BEST, dst_size_in_pixel.width(),
        dst_size_in_pixel.height());
  } else {
    scaled_bitmap = source;
  }
  if (color_type == kN32_SkColorType) {
    DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
    callback.Run(scaled_bitmap, content::READBACK_SUCCESS);
    return;
  }
  DCHECK_EQ(color_type, kAlpha_8_SkColorType);
  // The software path currently always returns N32 bitmap regardless of the
  // |color_type| we ask for.
  DCHECK_EQ(scaled_bitmap.colorType(), kN32_SkColorType);
  // Paint |scaledBitmap| to alpha-only |grayscale_bitmap|.
  SkBitmap grayscale_bitmap;
  bool success = grayscale_bitmap.tryAllocPixels(
      SkImageInfo::MakeA8(scaled_bitmap.width(), scaled_bitmap.height()));
  if (!success) {
    callback.Run(SkBitmap(), content::READBACK_BITMAP_ALLOCATION_FAILURE);
    return;
  }
  SkCanvas canvas(grayscale_bitmap);
  canvas.clear(SK_ColorBLACK);
  SkPaint paint;
  paint.setColorFilter(SkLumaColorFilter::Make());
  canvas.drawBitmap(scaled_bitmap, SkIntToScalar(0), SkIntToScalar(0), &paint);
  callback.Run(grayscale_bitmap, content::READBACK_SUCCESS);
}

}  // namespace

namespace content {

viz::FrameSinkId AllocateFrameSinkId() {
#if defined(OS_ANDROID)
  return CompositorImpl::AllocateFrameSinkId();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetContextFactoryPrivate()->AllocateFrameSinkId();
#endif
}

viz::FrameSinkManagerImpl* GetFrameSinkManager() {
#if defined(OS_ANDROID)
  return CompositorImpl::GetFrameSinkManager();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  if (!factory)
    return nullptr;
  return factory->GetContextFactoryPrivate()->GetFrameSinkManager();
#endif
}

viz::HostFrameSinkManager* GetHostFrameSinkManager() {
#if defined(OS_ANDROID)
  return CompositorImpl::GetHostFrameSinkManager();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  if (!factory)
    return nullptr;
  return factory->GetContextFactoryPrivate()->GetHostFrameSinkManager();
#endif
}

void CopyFromCompositingSurfaceHasResult(
    const gfx::Size& dst_size_in_pixel,
    const SkColorType color_type,
    const ReadbackRequestCallback& callback,
    std::unique_ptr<viz::CopyOutputResult> result) {
  if (result->IsEmpty()) {
    callback.Run(SkBitmap(), READBACK_FAILED);
    return;
  }

  gfx::Size output_size_in_pixel;
  if (dst_size_in_pixel.IsEmpty())
    output_size_in_pixel = result->size();
  else
    output_size_in_pixel = dst_size_in_pixel;

  switch (result->format()) {
    case viz::CopyOutputResult::Format::RGBA_TEXTURE:
      // TODO(miu): Delete this code path. All callers want a SkBitmap result;
      // so all requests should be changed to RGBA_BITMAP, and then not bother
      // with the extra GLHelper readback infrastructure client-side.
      // http://crbug.com/759310
      PrepareTextureCopyOutputResult(output_size_in_pixel, color_type, callback,
                                     std::move(result));
      break;

    case viz::CopyOutputResult::Format::RGBA_BITMAP:
      PrepareBitmapCopyOutputResult(output_size_in_pixel, color_type, callback,
                                    std::move(result));
      break;
  }
}

namespace surface_utils {

void ConnectWithInProcessFrameSinkManager(
    viz::HostFrameSinkManager* host,
    viz::FrameSinkManagerImpl* manager,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // A mojo pointer to |host| which is the FrameSinkManagerImpl's client.
  viz::mojom::FrameSinkManagerClientPtr host_mojo;
  // A mojo pointer to |manager|.
  viz::mojom::FrameSinkManagerPtr manager_mojo;

  // A request to bind to each of the above interfaces.
  viz::mojom::FrameSinkManagerClientRequest host_mojo_request =
      mojo::MakeRequest(&host_mojo);
  viz::mojom::FrameSinkManagerRequest manager_mojo_request =
      mojo::MakeRequest(&manager_mojo);

  // Sets |manager_mojo| which is given to the |host|.
  manager->BindAndSetClient(std::move(manager_mojo_request), task_runner,
                            std::move(host_mojo));
  // Sets |host_mojo| which was given to the |manager|.
  host->BindAndSetManager(std::move(host_mojo_request), task_runner,
                          std::move(manager_mojo));
}

void ConnectWithLocalFrameSinkManager(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    viz::FrameSinkManagerImpl* frame_sink_manager_impl) {
  host_frame_sink_manager->SetLocalManager(frame_sink_manager_impl);
  frame_sink_manager_impl->SetLocalClient(host_frame_sink_manager);
}

}  // namespace surface_utils

}  // namespace content
