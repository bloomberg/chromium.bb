// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_H_

namespace content {
class ResourceContext;
}

namespace prefetch {

// To be executed on the IO thread only.
bool IsPrefetchEnabled(content::ResourceContext* resource_context);

}  // namespace prefetch

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_H_
