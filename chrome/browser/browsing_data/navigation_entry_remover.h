// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"

class Profile;

namespace browsing_data {

// Remove navigation entries from the tabs of all browsers of |profile|.
// If a valid time_range is supplied, all entries within this time range will be
// removed and |deleted_rows| is ignored.
// Otherwise entries matching |deleted_rows| will be deleted.
void RemoveNavigationEntries(Profile* profile,
                             const history::DeletionTimeRange& time_range,
                             const history::URLRows& deleted_rows);

}  // namespace browsing_data

#endif  // CHROME_BROWSER_BROWSING_DATA_NAVIGATION_ENTRY_REMOVER_H_
