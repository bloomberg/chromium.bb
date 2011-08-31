// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/background_printing_manager.h"

#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace printing {

BackgroundPrintingManager::BackgroundPrintingManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BackgroundPrintingManager::~BackgroundPrintingManager() {
  DCHECK(CalledOnValidThread());
  // The might be some TabContentsWrappers still in |printing_tabs_| at
  // this point. E.g. when the last remaining tab is a print preview tab and
  // tries to print. In which case it will fail to print.
  // TODO(thestig) handle this case better.
}

void BackgroundPrintingManager::OwnPrintPreviewTab(
    TabContentsWrapper* preview_tab) {
  DCHECK(CalledOnValidThread());
  DCHECK(PrintPreviewTabController::IsPrintPreviewTab(preview_tab));
  CHECK(printing_tabs_.find(preview_tab) == printing_tabs_.end());

  printing_tabs_.insert(preview_tab);

  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                 Source<TabContentsWrapper>(preview_tab));
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(preview_tab->tab_contents()));

  // Detach |preview_tab| from its tab strip.
  Browser* browser = BrowserList::FindBrowserWithID(
      preview_tab->restore_tab_helper()->window_id().id());
  DCHECK(browser);

  TabStripModel* tabstrip = browser->tabstrip_model();
  tabstrip->DetachTabContentsAt(tabstrip->GetIndexOfTabContents(preview_tab));

  // Activate the initiator tab.
  PrintPreviewTabController* tab_controller =
      PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return;
  TabContentsWrapper* initiator_tab =
      tab_controller->GetInitiatorTab(preview_tab);
  if (!initiator_tab)
    return;
  static_cast<RenderViewHostDelegate*>(
      initiator_tab->tab_contents())->Activate();
}

void BackgroundPrintingManager::Observe(int type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PRINT_JOB_RELEASED: {
      TabContentsWrapper* tab = Source<TabContentsWrapper>(source).ptr();
      registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                        Source<TabContentsWrapper>(tab));

      // This might be happening in the middle of a RenderViewGone() loop.
      // Deleting |contents| later so the RenderViewGone() loop can finish.
      MessageLoop::current()->DeleteSoon(FROM_HERE, tab);
      break;
    }
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      TabContentsWrapper* tab =
          TabContentsWrapper::GetCurrentWrapperForContents(
              Source<TabContents>(source).ptr());
      if (registrar_.IsRegistered(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                                  Source<TabContentsWrapper>(tab))) {
        registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                          Source<TabContentsWrapper>(tab));
      }
      registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                        Source<TabContents>(tab->tab_contents()));
      printing_tabs_.erase(tab);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

BackgroundPrintingManager::TabContentsWrapperSet::const_iterator
    BackgroundPrintingManager::begin() {
  return printing_tabs_.begin();
}

BackgroundPrintingManager::TabContentsWrapperSet::const_iterator
    BackgroundPrintingManager::end() {
  return printing_tabs_.end();
}

bool BackgroundPrintingManager::HasPrintPreviewTab(
    TabContentsWrapper* preview_tab) {
  return printing_tabs_.find(preview_tab) != printing_tabs_.end();
}

}  // namespace printing
