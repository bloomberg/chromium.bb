// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
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
class SadTabHelper : public content::WebContentsObserver,
                     public content::NotificationObserver {
 public:
  explicit SadTabHelper(content::WebContents* web_contents);
  virtual ~SadTabHelper();

  // Platform specific function to determine if there is a current sad tab page.
  bool HasSadTab();

#if defined(TOOLKIT_VIEWS)
  views::Widget* sad_tab() { return sad_tab_.get(); }
#endif

 private:
  // Platform specific function to get an instance of the sad tab page.
  void InstallSadTab(base::TerminationStatus status);

  // Overridden from content::WebContentsObserver:
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

  DISALLOW_COPY_AND_ASSIGN(SadTabHelper);
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
