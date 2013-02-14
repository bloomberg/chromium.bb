// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

class Browser;

namespace content {
class WebContents;
}

// InstantUnloadHandler ensures that it runs the BeforeUnload/Unload Handlers
// (BUH) of a page if the page is replaced by an Instant overlay.
//
// Why is this needed? Say the user is looking at a page P. They then try to
// navigate to another page Q. Consider what happens with and without Instant:
//
// Without Instant: Before the navigation is committed, P's BUH are run. If P's
// BUH return a string (instead of the default null), the user is prompted to
// "Stay or Leave?". If the user clicks "Stay", the navigation is cancelled,
// and the user remains on P.
//
// With Instant: The navigation to Q has already happened, since Q is being
// shown as a preview (overlay). When the user "commits" the overlay, it's too
// late to cancel Q based on P's BUH. So, Instant just replaces P with Q and
// passes P to InstantUnloadHandler::RunUnloadListenersOrDestroy(). This class
// runs P's BUH in the background. If the "Stay or Leave?" dialog needs to be
// shown, it adds P back onto the tabstrip, next to Q. Otherwise, P is deleted.
class InstantUnloadHandler {
 public:
  explicit InstantUnloadHandler(Browser* browser);
  ~InstantUnloadHandler();

  // See class description for details on what this does.
  void RunUnloadListenersOrDestroy(scoped_ptr<content::WebContents> contents,
                                   int index);

 private:
  class WebContentsDelegateImpl;

  // Invoked if the tab is to be shown, at |index| on the tab strip. This
  // happens if the beforeunload listener returns a string.
  void Activate(WebContentsDelegateImpl* delegate,
                scoped_ptr<content::WebContents> contents,
                int index);

  // Destroys the old tab. This is invoked if script tries to close the page.
  void Destroy(WebContentsDelegateImpl* delegate);

  // TODO(sky): Browser really needs to wait to close until there are no more
  // tabs managed by InstantUnloadHandler.
  Browser* const browser_;

  ScopedVector<WebContentsDelegateImpl> delegates_;

  DISALLOW_COPY_AND_ASSIGN(InstantUnloadHandler);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
