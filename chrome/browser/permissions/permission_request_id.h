// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_ID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_ID_H_

#include <string>

#include "url/gurl.h"

// Uniquely identifies a particular permission request.
// None of the different attribute (render_process_id, render_frame_id,
// request_id or origin) is enough to compare two requests. In order to check if
// a request is the same as another one, consumers of this class should use
// the operator== or operator!=.
class PermissionRequestID {
 public:
  PermissionRequestID(int render_process_id,
                      int render_frame_id,
                      int request_id,
                      const GURL& origin);
  ~PermissionRequestID();

  PermissionRequestID(const PermissionRequestID&) = default;
  PermissionRequestID& operator=(const PermissionRequestID&) = default;

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }
  int request_id() const { return request_id_; }
  GURL origin() const { return origin_; }

  bool operator==(const PermissionRequestID& other) const;
  bool operator!=(const PermissionRequestID& other) const;

  std::string ToString() const;

 private:
  int render_process_id_;
  int render_frame_id_;
  int request_id_;

  // Needed for permission checks that are based on origin. It can be an empty
  // GURL is the request isn't checking origin.
  GURL origin_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_REQUEST_ID_H_
