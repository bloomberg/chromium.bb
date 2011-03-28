// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
#pragma once

#include "base/memory/scoped_vector.h"

class Browser;
class TabContentsWrapper;

// InstantUnloadHandler makes sure the before unload and unload handler is run
// when using instant. When the user commits the instant preview the existing
// TabContentsWrapper is passed to |RunUnloadListenersOrDestroy|. If the tab has
// no before unload or unload listener the tab is deleted, otherwise the before
// unload and unload listener is executed. If the before unload listener shows a
// dialog the tab is added back to the tabstrip at its original location next to
// the instant page.
class InstantUnloadHandler {
 public:
  explicit InstantUnloadHandler(Browser* browser);
  ~InstantUnloadHandler();

  // See class description for details on what this does.
  void RunUnloadListenersOrDestroy(TabContentsWrapper* tab_contents, int index);

 private:
  class TabContentsDelegateImpl;

  // Invoked if the tab is to be shown. This happens if the before unload
  // listener returns a string.
  void Activate(TabContentsDelegateImpl* delegate);

  // Destroys the old tab. This is invoked if script tries to close the page.
  void Destroy(TabContentsDelegateImpl* delegate);

  // TODO(sky): browser really needs to wait to close until there are no more
  // tabs managed by InstantUnloadHandler.
  Browser* browser_;

  ScopedVector<TabContentsDelegateImpl> delegates_;

  DISALLOW_COPY_AND_ASSIGN(InstantUnloadHandler);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_UNLOAD_HANDLER_H_
