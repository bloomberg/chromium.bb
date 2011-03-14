// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_FACTORIES_H_
#define MEDIA_BASE_FILTER_FACTORIES_H_

#include<string>

#include "base/callback.h"
#include "media/base/pipeline_status.h"

namespace media {

class DataSource;

// Asynchronous factory interface for building DataSource objects.
class DataSourceFactory {
 public:
  // Ownership of the DataSource is transferred through this callback.
  typedef Callback2<PipelineError, DataSource*>::Type BuildCallback;

  virtual ~DataSourceFactory();

  // Builds a DataSource for |url| and returns it via |callback|.
  virtual void Build(const std::string& url, BuildCallback* callback) = 0;

  // Makes a copy of this factory.
  // NOTE: Pending requests are not cloned.
  virtual DataSourceFactory* Clone() const = 0;
};

class Demuxer;

// Asynchronous factory interface for building Demuxer objects.
class DemuxerFactory {
 public:
  // Ownership of the Demuxer is transferred through this callback.
  typedef Callback2<PipelineError, Demuxer*>::Type BuildCallback;

  virtual ~DemuxerFactory();

  // Builds a Demuxer for |url| and returns it via |callback|.
  virtual void Build(const std::string& url, BuildCallback* callback) = 0;

  // Makes a copy of this factory.
  // NOTE: Pending requests are not cloned.
  virtual DemuxerFactory* Clone() const = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_FACTORIES_H_
