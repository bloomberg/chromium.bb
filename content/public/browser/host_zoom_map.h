// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
#define CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;
class ResourceContext;

// Maps hostnames to custom zoom levels.  Written on the UI thread and read on
// any thread.  One instance per browser context. Must be created on the UI
// thread, and it'll delete itself on the UI thread as well.
class HostZoomMap {
 public:
  CONTENT_EXPORT static HostZoomMap* GetForBrowserContext(
      BrowserContext* browser_context);

  // Copy the zoom levels from the given map. Can only be called on the UI
  // thread.
  virtual void CopyFrom(HostZoomMap* copy) = 0;

  // Returns the zoom level for the host or spec for a given url. The zoom
  // level is determined by the host portion of the URL, or (in the absence of
  // a host) the complete spec of the URL. In most cases, there is no custom
  // zoom level, and this returns the user's default zoom level.  Otherwise,
  // returns the saved zoom level, which may be positive (to zoom in) or
  // negative (to zoom out).
  //
  // This may be called on any thread.
  virtual double GetZoomLevel(const std::string& host) const = 0;

  // Sets the zoom level for the host or spec for a given url to |level|.  If
  // the level matches the current default zoom level, the host is erased
  // from the saved preferences; otherwise the new value is written out.
  //
  // This should only be called on the UI thread.
  virtual void SetZoomLevel(const std::string& host, double level) = 0;

  // Get/Set the default zoom level for pages that don't override it.
  virtual double GetDefaultZoomLevel() const = 0;
  virtual void SetDefaultZoomLevel(double level) = 0;;

 protected:
  virtual ~HostZoomMap() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HOST_ZOOM_MAP_H_
