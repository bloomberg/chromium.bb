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
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace printing {

BackgroundPrintingManager::BackgroundPrintingManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BackgroundPrintingManager::~BackgroundPrintingManager() {
  DCHECK(CalledOnValidThread());
  // The might be some TabContentsWrappers still in |printing_contents_| at
  // this point. E.g. when the last remaining tab is a print preview tab and
  // tries to print. In which case it will fail to print.
  // TODO(thestig) handle this case better.
}

void BackgroundPrintingManager::OwnTabContents(TabContentsWrapper* contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(printing::PrintPreviewTabController::IsPrintPreviewTab(
      contents->tab_contents()));
  CHECK(printing_contents_.find(contents) == printing_contents_.end());

  printing_contents_.insert(contents);

  registrar_.Add(this, NotificationType::PRINT_JOB_RELEASED,
                 Source<TabContentsWrapper>(contents));
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(contents->tab_contents()));

  // Detach |contents| from its tab strip.
  Browser* browser = BrowserList::FindBrowserWithID(
      contents->restore_tab_helper()->window_id().id());
  TabStripModel* tabstrip = browser->tabstrip_model();
  tabstrip->DetachTabContentsAt(tabstrip->GetIndexOfTabContents(contents));

  // Activate the initiator tab.
  printing::PrintPreviewTabController* tab_controller =
      printing::PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return;
  TabContents* initiator_tab = tab_controller->GetInitiatorTab(
      contents->tab_contents());
  if (!initiator_tab)
    return;
  initiator_tab->Activate();
}

void BackgroundPrintingManager::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PRINT_JOB_RELEASED: {
      // This might be happening in the middle of a RenderViewGone() loop.
      // Deleting |contents| later so the RenderViewGone() loop can finish.
      MessageLoop::current()->DeleteSoon(
          FROM_HERE,
          Source<TabContentsWrapper>(source).ptr());
      break;
    }
    case NotificationType::TAB_CONTENTS_DESTROYED: {
      TabContentsWrapper* tab =
          TabContentsWrapper::GetCurrentWrapperForContents(
              Source<TabContents>(source).ptr());
      registrar_.Remove(this, NotificationType::PRINT_JOB_RELEASED,
                        Source<TabContentsWrapper>(tab));
      registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                        Source<TabContents>(tab->tab_contents()));
      printing_contents_.erase(tab);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

std::set<TabContentsWrapper*>::const_iterator
    BackgroundPrintingManager::begin() {
  return printing_contents_.begin();
}

std::set<TabContentsWrapper*>::const_iterator
    BackgroundPrintingManager::end() {
  return printing_contents_.end();
}

bool BackgroundPrintingManager::HasTabContents(TabContentsWrapper* entry) {
  return printing_contents_.find(entry) != printing_contents_.end();
}

}  // namespace printing
