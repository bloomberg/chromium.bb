// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_REGISTRAR_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_REGISTRAR_H_

#include "content/browser/tab_contents/tab_contents_observer.h"

// Use this as a member variable in a class that uses the empty constructor
// version of this interface.  On destruction of TabContents being observed,
// the registrar must either be destroyed or explicitly set to observe
// another TabContents.
class TabContentsObserverRegistrar : public TabContentsObserver {
 public:
  explicit TabContentsObserverRegistrar(TabContentsObserver* observer);
  ~TabContentsObserverRegistrar();

  // Call this to start observing a tab.  Passing in NULL resets it.
  // This can only be used to watch one tab at a time.  If you call this and
  // you're already observing another tab, the old tab won't be observed
  // afterwards.
  void Observe(TabContents* tab);

 private:
  virtual void TabContentsDestroyed(TabContents* tab);

  TabContentsObserver* observer_;
  TabContents* tab_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsObserverRegistrar);
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TAB_CONTENTS_OBSERVER_REGISTRAR_H_
