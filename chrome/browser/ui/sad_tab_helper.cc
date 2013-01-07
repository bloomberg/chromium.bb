// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/sad_tab.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "ui/views/widget/widget.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SadTabHelper);

SadTabHelper::~SadTabHelper() {
}

SadTabHelper::SadTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_CONNECTED,
                 content::Source<content::WebContents>(web_contents));
}

void SadTabHelper::RenderViewGone(base::TerminationStatus status) {
  // Only show the sad tab if we're not in browser shutdown, so that WebContents
  // objects that are not in a browser (e.g., HTML dialogs) and thus are
  // visible do not flash a sad tab page.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  if (sad_tab_)
    return;

  if (status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED ||
      status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    InstallSadTab(status);
}

void SadTabHelper::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_CONNECTED, type);
  if (sad_tab_) {
#if defined(TOOLKIT_VIEWS)
    sad_tab_->Close();
    // See http://crbug.com/117668. When the Widget is being destructed, we
    // want calls to sad_tab() to return NULL.
    scoped_ptr<views::Widget> local_sad_tab;
    local_sad_tab.swap(sad_tab_);
#elif defined(TOOLKIT_GTK) || defined(OS_MACOSX)
    sad_tab_->Close();
#else
#error Unknown platform
#endif
    sad_tab_.reset();
  }
}

void SadTabHelper::InstallSadTab(base::TerminationStatus status) {
  chrome::SadTabKind kind =
      (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) ?
          chrome::SAD_TAB_KIND_KILLED : chrome::SAD_TAB_KIND_CRASHED;
#if defined(TOOLKIT_VIEWS)
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
#elif defined(TOOLKIT_GTK) || defined(OS_MACOSX)
  sad_tab_.reset(chrome::SadTab::Create(web_contents(), kind));
  sad_tab_->Show();
#else
#error Unknown platform
#endif
}
