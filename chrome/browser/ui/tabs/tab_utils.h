// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

namespace content{
class WebContents;
}  // namespace content

namespace chrome {

// Returns whether we should show a projecting favicon indicator for this tab.
bool ShouldShowProjectingIndicator(content::WebContents* contents);

// Returns whether we should show a recording favicon indicator for this tab.
bool ShouldShowRecordingIndicator(content::WebContents* contents);

// Returns whether the given |contents| is playing audio. We might choose to
// show an audio favicon indicator for this tab.
bool IsPlayingAudio(content::WebContents* contents);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
