// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/paint_worklet_image_cache.h"
#include "cc/paint/paint_worklet_layer_painter.h"

namespace cc {

// TODO(xidachen): Rename this to PaintWorkletTaskImpl.
class PaintWorkletImageCacheImpl : public TileTask {
 public:
  PaintWorkletImageCacheImpl(PaintWorkletImageCache* cache,
                             const PaintImage& paint_image)
      : TileTask(true), cache_(cache), paint_image_(paint_image) {}

  // Overridden from Task:
  void RunOnWorkerThread() override { cache_->PaintImageInTask(paint_image_); }

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~PaintWorkletImageCacheImpl() override = default;

 private:
  PaintWorkletImageCache* cache_;
  PaintImage paint_image_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletImageCacheImpl);
};

PaintWorkletImageCache::PaintWorkletImageCache() {}

PaintWorkletImageCache::~PaintWorkletImageCache() {}

void PaintWorkletImageCache::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  painter_ = std::move(painter);
}

scoped_refptr<TileTask> PaintWorkletImageCache::GetTaskForPaintWorkletImage(
    const DrawImage& image) {
  return base::MakeRefCounted<PaintWorkletImageCacheImpl>(this,
                                                          image.paint_image());
}

// TODO(xidachen): dispatch the work to a worklet thread, invoke JS callback.
// Do check the cache first. If there is already a cache entry for this input,
// then there is no need to call the Paint() function.
void PaintWorkletImageCache::PaintImageInTask(const PaintImage& paint_image) {
  sk_sp<PaintRecord> record = painter_->Paint();
  records_[paint_image.paint_worklet_input()] = record;
}

PaintRecord* PaintWorkletImageCache::GetPaintRecordForTest(
    PaintWorkletInput* input) {
  return records_[input].get();
}

}  // namespace cc
