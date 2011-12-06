// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_observer.h"

#include "chrome/browser/browser_shutdown.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#elif defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#elif defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/sad_tab_gtk.h"
#endif

SadTabObserver::SadTabObserver(TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_CONNECTED,
                 content::Source<TabContents>(tab_contents));
}

SadTabObserver::~SadTabObserver() {
}

void SadTabObserver::RenderViewGone(base::TerminationStatus status) {
  // Only show the sad tab if we're not in browser shutdown, so that TabContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  if (HasSadTab())
    return;

  gfx::NativeView view = AcquireSadTab(status);
  tab_contents()->view()->InstallOverlayView(view);
}

void SadTabObserver::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_TAB_CONTENTS_CONNECTED:
      if (HasSadTab()) {
        tab_contents()->view()->RemoveOverlayView();
        ReleaseSadTab();
      }
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

gfx::NativeView SadTabObserver::AcquireSadTab(base::TerminationStatus status) {
#if defined(OS_MACOSX)
  sad_tab_.reset(
      sad_tab_controller_mac::CreateSadTabController(tab_contents()));
  return sad_tab_controller_mac::GetViewOfSadTabController(sad_tab_.get());
#elif defined(TOOLKIT_VIEWS)
  SadTabView::Kind kind =
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
      SadTabView::KILLED : SadTabView::CRASHED;
  views::Widget::InitParams sad_tab_params(
      views::Widget::InitParams::TYPE_CONTROL);
  // It is not possible to create a native_widget_win that has no parent in
  // and later re-parent it.
  // TODO(avi): This is a cheat. Can this be made cleaner?
  sad_tab_params.parent_widget =
      static_cast<TabContentsViewViews*>(tab_contents()->view());
  sad_tab_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  sad_tab_.reset(new views::Widget);
  sad_tab_->Init(sad_tab_params);
  sad_tab_->SetContentsView(new SadTabView(tab_contents(), kind));
  return sad_tab_->GetNativeView();
#elif defined(TOOLKIT_GTK)
  sad_tab_.reset(new SadTabGtk(
      tab_contents(),
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED
          ? SadTabGtk::KILLED
          : SadTabGtk::CRASHED));
  return sad_tab_->widget();
#else
#error Unknown platform
#endif
}

void SadTabObserver::ReleaseSadTab() {
  sad_tab_.reset();
}

bool SadTabObserver::HasSadTab() {
  return sad_tab_.get() != NULL;
}
