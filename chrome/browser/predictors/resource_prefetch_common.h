// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_
#pragma once

#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace content {
class WebContents;
}

namespace predictors {

// Represents a single navigation for a render view.
struct NavigationID {
  // TODO(shishir): Maybe take process_id, view_id and url as input in
  // constructor.
  NavigationID();
  NavigationID(const NavigationID& other);
  explicit NavigationID(const content::WebContents& web_contents);
  bool operator<(const NavigationID& rhs) const;
  bool operator==(const NavigationID& rhs) const;

  bool IsSameRenderer(const NavigationID& other) const;

  // Returns true iff the render_process_id_, render_view_id_ and
  // main_frame_url_ has been set correctly.
  bool is_valid() const;

  int render_process_id;
  int render_view_id;
  GURL main_frame_url;

  // NOTE: Even though we store the creation time here, it is not used during
  // comparison of two NavigationIDs because it cannot always be determined
  // correctly.
  base::TimeTicks creation_time;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_COMMON_H_
