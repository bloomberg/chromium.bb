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
#include "content/browser/renderer_host/render_view_host.h"
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
  CHECK(!HasPrintPreviewTab(preview_tab));

  printing_tabs_.insert(preview_tab);

  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                 Source<TabContentsWrapper>(preview_tab));

  // OwnInitiatorTabContents() may have already added this notification.
  TabContents* preview_contents = preview_tab->tab_contents();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                               Source<TabContents>(preview_contents))) {
    registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(preview_contents));
  }

  // If a tab that is printing crashes, the user cannot destroy it since it is
  // not in any tab strip. Thus listen for crashes here and delete the tab.
  //
  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  RenderProcessHost* rph = preview_tab->render_view_host()->process();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                               Source<RenderProcessHost>(rph))) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   Source<RenderProcessHost>(rph));
  }

  RemoveFromTabStrip(preview_tab);

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

bool BackgroundPrintingManager::OwnInitiatorTab(
    TabContentsWrapper* initiator_tab) {
  DCHECK(CalledOnValidThread());
  DCHECK(!PrintPreviewTabController::IsPrintPreviewTab(initiator_tab));
  bool has_initiator_tab = false;
  for (TabContentsWrapperMap::iterator it = map_.begin(); it != map_.end();
       ++it) {
    if (it->second == initiator_tab) {
      has_initiator_tab = true;
      break;
    }
  }
  CHECK(!has_initiator_tab);

  PrintPreviewTabController* tab_controller =
      PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return false;
  TabContentsWrapper* preview_tab =
      tab_controller->GetPrintPreviewForTab(initiator_tab);
  if (!preview_tab)
    return false;

  map_[preview_tab] = initiator_tab;

  // OwnPrintPreviewTab() may have already added this notification.
  TabContents* preview_contents = preview_tab->tab_contents();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                               Source<TabContents>(preview_contents))) {
    registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(preview_contents));
  }

  RemoveFromTabStrip(initiator_tab);
  return true;
}

void BackgroundPrintingManager::Observe(int type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      OnRendererProcessClosed(Source<RenderProcessHost>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_PRINT_JOB_RELEASED: {
      OnPrintJobReleased(Source<TabContentsWrapper>(source).ptr());
      break;
    }
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      OnTabContentsDestroyed(
          TabContentsWrapper::GetCurrentWrapperForContents(
              Source<TabContents>(source).ptr()));
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void BackgroundPrintingManager::OnRendererProcessClosed(
    RenderProcessHost* rph) {
  TabContentsWrapperSet::const_iterator it;
  for (it = begin(); it != end(); ++it) {
    TabContentsWrapper* tab = *it;
    if (tab->render_view_host()->process() == rph)
      MessageLoop::current()->DeleteSoon(FROM_HERE, tab);
  }
}

void BackgroundPrintingManager::OnPrintJobReleased(
    TabContentsWrapper* preview_tab) {
  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                    Source<TabContentsWrapper>(preview_tab));

  // This might be happening in the middle of a RenderViewGone() loop.
  // Deleting |contents| later so the RenderViewGone() loop can finish.
  MessageLoop::current()->DeleteSoon(FROM_HERE, preview_tab);
}

void BackgroundPrintingManager::OnTabContentsDestroyed(
    TabContentsWrapper* preview_tab) {
  bool is_owned_printing_tab = HasPrintPreviewTab(preview_tab);
  bool is_preview_tab_for_owned_initator_tab =
      (map_.find(preview_tab) != map_.end());
  DCHECK(is_owned_printing_tab || is_preview_tab_for_owned_initator_tab);

  // Always need to remove this notification since the tab is gone.
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(preview_tab->tab_contents()));

  // Delete the associated initiator tab if one exists.
  if (is_preview_tab_for_owned_initator_tab) {
    TabContentsWrapper* initiator_tab = map_[preview_tab];
    map_.erase(preview_tab);
    MessageLoop::current()->DeleteSoon(FROM_HERE, initiator_tab);
  }

  // If |preview_tab| is not owned, then we are done.
  if (!is_owned_printing_tab)
    return;

  // Remove NOTIFICATION_RENDERER_PROCESS_CLOSED if |tab| is the last
  // TabContents associated with |rph|.
  bool shared_rph = false;
  RenderProcessHost* rph = preview_tab->render_view_host()->process();
  TabContentsWrapperSet::const_iterator it;
  for (it = begin(); it != end(); ++it) {
    TabContentsWrapper* iter_tab = *it;
    if (iter_tab == preview_tab)
      continue;
    if (iter_tab->render_view_host()->process() == rph) {
      shared_rph = true;
      break;
    }
  }
  if (!shared_rph) {
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      Source<RenderProcessHost>(rph));
  }

  // Remove other notifications and remove the tab from |printing_tabs_|.
  if (registrar_.IsRegistered(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                              Source<TabContentsWrapper>(preview_tab))) {
    registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                      Source<TabContentsWrapper>(preview_tab));
  }
  printing_tabs_.erase(preview_tab);
}

void BackgroundPrintingManager::RemoveFromTabStrip(TabContentsWrapper* tab) {
  Browser* browser = BrowserList::FindBrowserWithID(
      tab->restore_tab_helper()->window_id().id());
  DCHECK(browser);

  TabStripModel* tabstrip = browser->tabstrip_model();
  int index = tabstrip->GetIndexOfTabContents(tab);
  if (index == TabStripModel::kNoTab)
    return;
  tabstrip->DetachTabContentsAt(index);
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
