// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_

#include <string>

#include "url/gurl.h"

// Uniquely identifies a particular permission request.
class PermissionRequestID {
 public:
  PermissionRequestID(int render_process_id,
                      int render_view_id,
                      int bridge_id,
                      const GURL& origin);
  ~PermissionRequestID();

  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }
  int bridge_id() const { return bridge_id_; }
  GURL origin() const { return origin_; }

  bool Equals(const PermissionRequestID& other) const;
  bool IsForSameTabAs(const PermissionRequestID& other) const;
  std::string ToString() const;

 private:
  int render_process_id_;
  int render_view_id_;
  // Id unique to this instance.
  int bridge_id_;
  // Needed for permission checks that are based on origin.
  // If you don't use origin to check permission request, pass an empty GURL.
  GURL origin_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_REQUEST_ID_H_
