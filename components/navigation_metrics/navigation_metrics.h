// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_
#define COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_

class GURL;

namespace navigation_metrics {

void RecordMainFrameNavigation(const GURL& url,
                               bool is_same_document,
                               bool is_off_the_record);

}  // namespace navigation_metrics

#endif  // COMPONENTS_NAVIGATION_METRICS_NAVIGATION_METRICS_H_
