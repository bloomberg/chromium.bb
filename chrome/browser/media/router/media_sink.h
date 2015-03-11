// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_

#include <string>

#include "chrome/browser/media/router/media_route_id.h"

namespace media_router {

using MediaSinkId = std::string;

// Represents a sink to which media can be routed.
class MediaSink {
 public:
  // |sink_id|: Unique identifier for the MediaSink.
  // |name|: Descriptive name of the MediaSink.
  MediaSink(const MediaSinkId& sink_id,
            const std::string& name);
  ~MediaSink();

  const MediaSinkId& sink_id() const { return sink_id_; }
  const std::string& name() const { return name_; }

  bool Equals(const MediaSink& other) const;

 private:
  const MediaSinkId sink_id_;
  const std::string name_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
