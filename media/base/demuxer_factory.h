// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_FACTORY_H_
#define MEDIA_BASE_DEMUXER_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class Demuxer;

// Asynchronous factory interface for building Demuxer objects.
class MEDIA_EXPORT DemuxerFactory {
 public:
  // Ownership of the Demuxer is transferred through this callback.
  typedef base::Callback<void(PipelineStatus, Demuxer*)> BuildCallback;

  virtual ~DemuxerFactory();

  // Builds a Demuxer for |url| and returns it via |callback|.
  virtual void Build(const std::string& url, const BuildCallback& callback) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_FACTORY_H_
