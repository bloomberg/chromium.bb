// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_

#include "url/gurl.h"

namespace offline_pages {

// Shows a notification that informs the user of offline content available at
// |origin|'s host, and that when clicked opens Chrome's download manager.
void ShowPrefetchedContentNotification(const GURL& origin);

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCHED_PAGES_NOTIFIER_H_
