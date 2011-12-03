// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_GLOBAL_REQUEST_ID_H_
#define CONTENT_BROWSER_RENDERER_HOST_GLOBAL_REQUEST_ID_H_
#pragma once

// Uniquely identifies a net::URLRequest.
struct GlobalRequestID {
  GlobalRequestID() : child_id(-1), request_id(-1) {
  }

  GlobalRequestID(int child_id, int request_id)
      : child_id(child_id),
        request_id(request_id) {
  }

  // The unique ID of the child process (different from OS's PID).
  int child_id;

  // The request ID (unique for the child).
  int request_id;

  bool operator<(const GlobalRequestID& other) const {
    if (child_id == other.child_id)
      return request_id < other.request_id;
    return child_id < other.child_id;
  }
  bool operator==(const GlobalRequestID& other) const {
    return child_id == other.child_id &&
        request_id == other.request_id;
  }
  bool operator!=(const GlobalRequestID& other) const {
    return child_id != other.child_id ||
        request_id != other.request_id;
  }
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GLOBAL_REQUEST_ID_H_
