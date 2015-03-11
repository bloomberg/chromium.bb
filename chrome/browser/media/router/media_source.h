// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_

#include <string>

namespace media_router {

class MediaSource {
 public:
  explicit MediaSource(const std::string& id);
  ~MediaSource();

  // Gets the ID of the media source.
  std::string id() const;

 private:
  std::string id_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SOURCE_H_
