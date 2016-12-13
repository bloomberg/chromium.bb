// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_LISTENER_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_LISTENER_H_

class GURL;

namespace physical_web {

// Class for being notified when Physical Web data changes.
class PhysicalWebListener {
 public:

  // OnFound(url) will be called when a new URL has been found.
  virtual void OnFound(const GURL& url) = 0;

  // OnLost(url) will be called when a URL can no longer be seen.
  virtual void OnLost(const GURL& url) = 0;

  // OnDistanceChagned(url, distance_estimate) will be called when the distance
  // estimate is changed for the URL.
  virtual void OnDistanceChanged(const GURL& url, double distance_estimate) = 0;
};

}  // namespace physical_web

#endif // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_LISTENER_H_
