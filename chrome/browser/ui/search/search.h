// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_H_
#pragma once

class Profile;

namespace chrome {
namespace search {

// Returns whether the Instant extended API is enabled for the given |profile|.
// |profile| may not be NULL.
bool IsInstantExtendedAPIEnabled(const Profile* profile);

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_H_
