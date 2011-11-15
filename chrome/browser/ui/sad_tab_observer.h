// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_OBSERVER_H_
#define CHROME_BROWSER_UI_SAD_TAB_OBSERVER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_MACOSX)
class SadTabController;
#elif defined(TOOLKIT_VIEWS)
namespace views {
class Widget;
}
#elif defined(TOOLKIT_GTK)
class SadTabGtk;
#endif

// Per-tab class to manage sad tab views.
class SadTabObserver : public TabContentsObserver,
                       public content::NotificationObserver {
 public:
  explicit SadTabObserver(TabContents* tab_contents);
  virtual ~SadTabObserver();

 private:
  // Platform specific function to get an instance of the sad tab page.
  gfx::NativeView AcquireSadTab(base::TerminationStatus status);

  // Platform specific function to release the instance of the sad tab page.
  void ReleaseSadTab();

  // Platform specific function to determine if there is a current sad tab page.
  bool HasSadTab();

  // Overridden from TabContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Used to get notifications about renderers coming and going.
  content::NotificationRegistrar registrar_;

  // The platform views used to render the sad tab, non-NULL if visible.
#if defined(OS_MACOSX)
  class ScopedPtrRelease {
   public:
    inline void operator()(void* x) const {
      base::mac::NSObjectRelease(x);
    }
  };
  scoped_ptr_malloc<SadTabController, ScopedPtrRelease> sad_tab_;
#elif defined(TOOLKIT_VIEWS)
  scoped_ptr<views::Widget> sad_tab_;
#elif defined(TOOLKIT_GTK)
  scoped_ptr<SadTabGtk> sad_tab_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SadTabObserver);
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_OBSERVER_H_
