// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

class Demuxer;
class Renderer;
class TextRenderer;

// Represents a set of uninitialized demuxer and renderers. Used to start a
// Pipeline object for media playback.
//
// TODO(xhwang): Create TextRenderer in Pipeline, pass Demuxer and Renderer to
// Pipeline, and remove FilterCollection, see http://crbug.com/110800
class MEDIA_EXPORT FilterCollection {
 public:
  FilterCollection();
  ~FilterCollection();

  void SetDemuxer(Demuxer* demuxer);
  Demuxer* GetDemuxer();

  void SetRenderer(scoped_ptr<Renderer> renderer);
  scoped_ptr<Renderer> GetRenderer();

  void SetTextRenderer(scoped_ptr<TextRenderer> text_renderer);
  scoped_ptr<TextRenderer> GetTextRenderer();

 private:
  Demuxer* demuxer_;
  scoped_ptr<Renderer> renderer_;
  scoped_ptr<TextRenderer> text_renderer_;

  DISALLOW_COPY_AND_ASSIGN(FilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_COLLECTION_H_
