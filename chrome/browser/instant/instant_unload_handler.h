// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"

class Browser;

namespace content {
class WebContents;
}

// InstantUnloadHandler ensures that the beforeunload and unload handlers are
// run when using Instant. When the user commits the Instant preview the
// existing WebContents is passed to RunUnloadListenersOrDestroy(). If the tab
// has no beforeunload or unload listeners, the tab is deleted; otherwise the
// beforeunload and unload listeners are executed. If the beforeunload listener
// shows a dialog the tab is added back to the tabstrip at its original location
// next to the Instant page.
class InstantUnloadHandler {
 public:
  explicit InstantUnloadHandler(Browser* browser);
  ~InstantUnloadHandler();

  // See class description for details on what this does.
  void RunUnloadListenersOrDestroy(content::WebContents* contents, int index);

 private:
  class WebContentsDelegateImpl;

  // Invoked if the tab is to be shown, at |index| on the tab strip. This
  // happens if the before unload listener returns a string. Takes ownership of
  // |delegate| and |contents|.
  void Activate(WebContentsDelegateImpl* delegate,
                content::WebContents* contents,
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
