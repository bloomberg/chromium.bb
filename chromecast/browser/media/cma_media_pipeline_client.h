// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CMA_MEDIA_PIPELINE_CLIENT_H_
#define CHROMECAST_BROWSER_MEDIA_CMA_MEDIA_PIPELINE_CLIENT_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/base/cast_resource.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

struct MediaPipelineDeviceParams;

// Class to provide media backend and watch media pipeline status.
class CmaMediaPipelineClient : public base::RefCounted<CmaMediaPipelineClient>,
                               public CastResource {
 public:
  CmaMediaPipelineClient();

  virtual scoped_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const MediaPipelineDeviceParams& params);

  virtual void OnMediaPipelineBackendCreated();
  virtual void OnMediaPipelineBackendDestroyed();

  // cast::CastResource implementation:
  void ReleaseResource(CastResource::Resource resource) override;

 protected:
  ~CmaMediaPipelineClient() override;

 private:
  friend class base::RefCounted<CmaMediaPipelineClient>;

  // Number of created media pipelines.
  size_t media_pipeline_count_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CmaMediaPipelineClient);
};
}
}

#endif
