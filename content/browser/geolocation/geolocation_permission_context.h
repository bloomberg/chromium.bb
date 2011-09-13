// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;

// GeolocationPermissionContext must be implemented by the embedder, to provide
// the policy and logic for the Geolocation permissions flow.
// This includes both prompting the user and persisting results, as required.
class CONTENT_EXPORT GeolocationPermissionContext
    : public base::RefCountedThreadSafe<GeolocationPermissionContext> {
 public:
  // The renderer is requesting permission to use Geolocation.
  // When the answer to a permission request has been determined, the result
  // should be forwarded to the renderer via SetGeolocationPermissionResponse().
  virtual void RequestGeolocationPermission(int render_process_id,
                                            int render_view_id,
                                            int bridge_id,
                                            const GURL& requesting_frame) = 0;

  // The renderer is cancelling a pending permission request.
  virtual void CancelGeolocationPermissionRequest(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame) = 0;

  // The embedder must callback to this method when the outcome of a previous
  // geolocation request (indicated via RequestGeolocationPermission above) has
  // been determined.
  static void SetGeolocationPermissionResponse(int render_process_id,
                                               int render_view_id,
                                               int bridge_id,
                                               bool is_allowed);

 protected:
  GeolocationPermissionContext();
  virtual ~GeolocationPermissionContext();

 private:
  friend class base::RefCountedThreadSafe<GeolocationPermissionContext>;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
