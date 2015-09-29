// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CMA_MEDIA_PIPELINE_CLIENT_H_
#define CHROMECAST_BROWSER_MEDIA_CMA_MEDIA_PIPELINE_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chromecast/base/cast_resource.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

// Class to provide media backend and watch media pipeline status
class CmaMediaPipelineClient : public base::RefCounted<CmaMediaPipelineClient>,
                               public CastResource {
 public:
  CmaMediaPipelineClient();

  virtual scoped_ptr<MediaPipelineBackend> CreateMediaPipelineBackend(
      const media::MediaPipelineDeviceParams& params);

  virtual void OnMediaPipelineBackendCreated();
  virtual void OnMediaPipelineBackendDestroyed();

  // cast::CastResource implementations
  void ReleaseResource(CastResource::Resource resource) override;

 protected:
  ~CmaMediaPipelineClient() override;

 private:
  friend class base::RefCounted<CmaMediaPipelineClient>;
  DISALLOW_COPY_AND_ASSIGN(CmaMediaPipelineClient);
};
}
}

#endif
