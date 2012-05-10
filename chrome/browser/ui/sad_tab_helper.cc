// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_helper.h"

#include "chrome/browser/browser_shutdown.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/tab_contents/sad_tab_controller.h"
#elif defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "ui/views/widget/widget.h"
#elif defined(TOOLKIT_GTK)

#include <gtk/gtk.h>

#include "chrome/browser/tab_contents/chrome_web_contents_view_delegate_gtk.h"
#include "chrome/browser/ui/gtk/sad_tab_gtk.h"
#endif

using content::WebContents;

SadTabHelper::SadTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::Source<WebContents>(web_contents));
}

SadTabHelper::~SadTabHelper() {
}

void SadTabHelper::RenderViewGone(base::TerminationStatus status) {
  // Only show the sad tab if we're not in browser shutdown, so that WebContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  // Don't build the sad tab view when the termination status is normal.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;

  if (HasSadTab())
    return;

  InstallSadTab(status);
}

void SadTabHelper::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_CONNECTED:
      if (HasSadTab()) {
#if defined(OS_MACOSX)
        sad_tab_controller_mac::RemoveSadTab(sad_tab_.get());
#elif defined(TOOLKIT_VIEWS)
        sad_tab_->Close();
        // See http://crbug.com/117668. When the Widget is being destructed, we
        // want calls to sad_tab() to return NULL.
        scoped_ptr<views::Widget> local_sad_tab;
        local_sad_tab.swap(sad_tab_);
#elif defined(TOOLKIT_GTK)
        GtkWidget* expanded_container =
            ChromeWebContentsViewDelegateGtk::GetFor(web_contents())->
                expanded_container();
        gtk_container_remove(
            GTK_CONTAINER(expanded_container), sad_tab_->widget());
#else
#error Unknown platform
#endif
        sad_tab_.reset();
      }
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

void SadTabHelper::InstallSadTab(base::TerminationStatus status) {
#if defined(OS_MACOSX)
  sad_tab_.reset(
      sad_tab_controller_mac::CreateSadTabController(web_contents()));
#elif defined(TOOLKIT_VIEWS)
  SadTabView::Kind kind =
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ?
      SadTabView::KILLED : SadTabView::CRASHED;
  views::Widget::InitParams sad_tab_params(
      views::Widget::InitParams::TYPE_CONTROL);
  // It is not possible to create a native_widget_win that has no parent in
  // and later re-parent it.
  // TODO(avi): This is a cheat. Can this be made cleaner?
  sad_tab_params.parent = web_contents()->GetView()->GetNativeView();
#if defined(OS_WIN) && !defined(USE_AURA)
  // Crash data indicates we can get here when the parent is no longer valid.
  // Attempting to create a child window with a bogus parent crashes. So, we
  // don't show a sad tab in this case in hopes the tab is in the process of
  // shutting down.
  if (!IsWindow(sad_tab_params.parent))
    return;
#endif
  sad_tab_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  sad_tab_.reset(new views::Widget);
  sad_tab_->Init(sad_tab_params);
  sad_tab_->SetContentsView(new SadTabView(web_contents(), kind));

  views::Widget::ReparentNativeView(
      sad_tab_->GetNativeView(), web_contents()->GetView()->GetNativeView());
  gfx::Rect bounds;
  web_contents()->GetView()->GetContainerBounds(&bounds);
  sad_tab_->SetBounds(gfx::Rect(bounds.size()));
#elif defined(TOOLKIT_GTK)
  sad_tab_.reset(new SadTabGtk(
      web_contents(),
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED
          ? SadTabGtk::KILLED
          : SadTabGtk::CRASHED));
  GtkWidget* expanded_container =
      ChromeWebContentsViewDelegateGtk::GetFor(web_contents())->
          expanded_container();
  gtk_container_add(GTK_CONTAINER(expanded_container), sad_tab_->widget());
  gtk_widget_show(sad_tab_->widget());
#else
#error Unknown platform
#endif
}

bool SadTabHelper::HasSadTab() {
  return sad_tab_.get() != NULL;
}
