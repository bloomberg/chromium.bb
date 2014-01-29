// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_HELPERS_H_
#define CHROME_BROWSER_UI_TAB_HELPERS_H_

namespace content {
class WebContents;
}

namespace chrome {
class BrowserTabStripModelDelegate;

namespace android {
class ChromeWebContentsDelegateAndroid;
}
}

namespace prerender {
class PrerenderContents;
}

// A "tab contents" is a WebContents that is used as a tab in a browser window
// (or the equivalent on Android). The TabHelpers class allows specific classes
// to attach the set of tab helpers that is used for tab contents.
//
// TODO(avi): This list is rather large, and for most callers it's due to the
// fact that they need tab helpers attached early to deal with arbitrary
// content loaded into a WebContents that will later be added to the tabstrip.
// Is there a better way to handle this? (Ideally, this list would contain
// only Browser and BrowserTabStripModelDelegate.)
class TabHelpers {
 private:
  // Browser and its TabStripModelDelegate have intimate control of tabs.
  // TabAndroid is the equivalent on Android.
  friend class Browser;
  friend class chrome::BrowserTabStripModelDelegate;

  // These are the Android equivalents of the two classes above.
  friend class TabAndroid;
  friend class chrome::android::ChromeWebContentsDelegateAndroid;

  // chrome::Navigate creates WebContents that are destined for the tab strip,
  // and that might have WebUI that immediately calls back into random tab
  // helpers.
  friend class BrowserNavigatorWebContentsAdoption;

  // Prerendering loads pages that have arbitrary external content; it needs
  // the full set of tab helpers to deal with it.
  friend class prerender::PrerenderContents;

  // Adopts the specified WebContents as a full-fledged browser tab, attaching
  // all the associated tab helpers that are needed for the WebContents to
  // serve in that role. It is safe to call this on a WebContents that was
  // already adopted.
  static void AttachTabHelpers(content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_TAB_HELPERS_H_
