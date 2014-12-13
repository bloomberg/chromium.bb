// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATOR_CONNECT_TYPES_H_
#define CONTENT_COMMON_NAVIGATOR_CONNECT_TYPES_H_

#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

struct CrossOriginServiceWorkerClient {
  CrossOriginServiceWorkerClient();
  CrossOriginServiceWorkerClient(const GURL& target_url,
                                 const GURL& origin,
                                 int message_port_id);
  ~CrossOriginServiceWorkerClient();

  GURL target_url;
  GURL origin;
  int message_port_id;
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATOR_CONNECT_TYPES_H_
