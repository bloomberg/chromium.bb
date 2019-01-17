// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_

class GURL;
namespace optimization_guide {
namespace proto {
class Hint;
class Optimization;
class PageHint;
}  // namespace proto
}  // namespace optimization_guide

namespace previews {

// Returns true if the optimization is part of a disabled experiment.
bool IsDisabledExperimentalOptimization(
    const optimization_guide::proto::Optimization& optimization);

// Returns the matching PageHint for |gurl| if found in |hint|.
const optimization_guide::proto::PageHint* FindPageHintForURL(
    const GURL& gurl,
    const optimization_guide::proto::Hint* hint);

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_HINTS_UTIL_H_
