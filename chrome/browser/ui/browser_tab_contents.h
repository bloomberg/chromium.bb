// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_TAB_CONTENTS_H_
#define CHROME_BROWSER_UI_BROWSER_TAB_CONTENTS_H_

class Browser;

namespace content {
class WebContents;
}

namespace chrome {
class BrowserTabStripModelDelegate;
}

namespace prerender {
class PrerenderContents;
}

// A "tab contents" is a WebContents that is used as a tab in a browser
// window, and thus is owned by a Browser's TabStripModel. The
// BrowserTabContents class allows specific classes to attach the set of tab
// helpers that is used for tab contents.
//
// TODO(avi): This list is rather large, and for most callers it's due to the
// fact that they need tab helpers attached early to deal with arbitrary
// content loaded into a WebContents that will later be added to the tabstrip.
// Is there a better way to handle this? (Ideally, this list would contain
// only Browser and BrowserTabStripModelDelegate.)
class BrowserTabContents {
 private:
  // Browser and its TabStripModelDelegate have intimate control of tabs.
  // TabAndroid is the equivalent on Android.
  friend class Browser;
  friend class chrome::BrowserTabStripModelDelegate;
  friend class TabAndroid;

  // chrome::Navigate creates WebContents that are destined for the tab strip,
  // and that might have WebUI that immediately calls back into random tab
  // helpers.
  friend class BrowserNavigatorWebContentsAdoption;

  // ChromeFrame is defined as a complete tab of Chrome inside of an IE
  // window, so it need to have the full complement of tab helpers that it
  // would have if it were in a Browser.
  // TODO(avi): It's still probably a good idea for Chrome Frame to more
  // explicitly control which tab helpers get created for its WebContentses.
  // http://crbug.com/157590
  friend class ExternalTabContainerWin;

  // Prerendering loads pages that have arbitrary external content; it needs
  // the full set of tab helpers to deal with it.
  friend class prerender::PrerenderContents;

  // Adopts the specified WebContents as a full-fledged browser tab, attaching
  // all the associated tab helpers that are needed for the WebContents to
  // serve in that role. It is safe to call this on a WebContents that was
  // already adopted.
  static void AttachTabHelpers(content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_BROWSER_TAB_CONTENTS_H_
