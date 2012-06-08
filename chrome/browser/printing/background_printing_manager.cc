// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/background_printing_manager.h"

#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

using content::BrowserThread;
using content::WebContents;

namespace printing {

BackgroundPrintingManager::BackgroundPrintingManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

BackgroundPrintingManager::~BackgroundPrintingManager() {
  DCHECK(CalledOnValidThread());
  // The might be some TabContents still in |printing_tabs_| at
  // this point. E.g. when the last remaining tab is a print preview tab and
  // tries to print. In which case it will fail to print.
  // TODO(thestig) handle this case better.
}

void BackgroundPrintingManager::OwnPrintPreviewTab(TabContents* preview_tab) {
  DCHECK(CalledOnValidThread());
  DCHECK(PrintPreviewTabController::IsPrintPreviewTab(preview_tab));
  CHECK(!HasPrintPreviewTab(preview_tab));

  printing_tabs_.insert(preview_tab);

  content::Source<TabContents> preview_source(preview_tab);
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED, preview_source);

  // OwnInitiatorTabContents() may have already added this notification.
  if (!registrar_.IsRegistered(this,
      chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED, preview_source)) {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                   preview_source);
  }

  // If a tab that is printing crashes, the user cannot destroy it since it is
  // not in any tab strip. Thus listen for crashes here and delete the tab.
  //
  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::RenderProcessHost* rph =
      preview_tab->web_contents()->GetRenderProcessHost();
  if (!registrar_.IsRegistered(this,
                               content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                               content::Source<content::RenderProcessHost>(
                                  rph))) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   content::Source<content::RenderProcessHost>(rph));
  }

  // Activate the initiator tab.
  PrintPreviewTabController* tab_controller =
      PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return;
  TabContents* initiator_tab = tab_controller->GetInitiatorTab(preview_tab);
  if (!initiator_tab)
    return;
  WebContents* web_contents = initiator_tab->web_contents();
  web_contents->GetDelegate()->ActivateContents(web_contents);
}

void BackgroundPrintingManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      OnRendererProcessClosed(
          content::Source<content::RenderProcessHost>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_PRINT_JOB_RELEASED: {
      OnPrintJobReleased(content::Source<TabContents>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED: {
      OnTabContentsDestroyed(content::Source<TabContents>(source).ptr());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void BackgroundPrintingManager::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  TabContentsSet preview_tabs_pending_deletion;
  TabContentsSet::const_iterator it;
  for (it = begin(); it != end(); ++it) {
    TabContents* preview_tab = *it;
    if (preview_tab->web_contents()->GetRenderProcessHost() == rph) {
      preview_tabs_pending_deletion.insert(preview_tab);
    }
  }
  for (it = preview_tabs_pending_deletion.begin();
       it != preview_tabs_pending_deletion.end();
       ++it) {
    DeletePreviewTab(*it);
  }
}

void BackgroundPrintingManager::OnPrintJobReleased(TabContents* preview_tab) {
  DeletePreviewTab(preview_tab);
}

void BackgroundPrintingManager::OnTabContentsDestroyed(
    TabContents* preview_tab) {
  // Always need to remove this notification since the tab is gone.
  content::Source<TabContents> preview_source(preview_tab);
  registrar_.Remove(this, chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                    preview_source);

  if (!HasPrintPreviewTab(preview_tab)) {
    NOTREACHED();
    return;
  }

  // Remove NOTIFICATION_RENDERER_PROCESS_CLOSED if |preview_tab| is the last
  // WebContents associated with |rph|.
  bool shared_rph = HasSharedRenderProcessHost(printing_tabs_, preview_tab) ||
      HasSharedRenderProcessHost(printing_tabs_pending_deletion_, preview_tab);
  if (!shared_rph) {
    content::RenderProcessHost* rph =
        preview_tab->web_contents()->GetRenderProcessHost();
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      content::Source<content::RenderProcessHost>(rph));
  }

  // Remove other notifications and remove the tab from its TabContentsSet.
  if (printing_tabs_.find(preview_tab) != printing_tabs_.end()) {
    registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                      preview_source);
    printing_tabs_.erase(preview_tab);
  } else {
    // DeletePreviewTab already deleted the notification.
    printing_tabs_pending_deletion_.erase(preview_tab);
  }
}

void BackgroundPrintingManager::DeletePreviewTab(TabContents* tab) {
  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                    content::Source<TabContents>(tab));
  printing_tabs_.erase(tab);
  printing_tabs_pending_deletion_.insert(tab);
  MessageLoop::current()->DeleteSoon(FROM_HERE, tab);
}

bool BackgroundPrintingManager::HasSharedRenderProcessHost(
    const TabContentsSet& set,
    TabContents* tab) {
  content::RenderProcessHost* rph = tab->web_contents()->GetRenderProcessHost();
  for (TabContentsSet::const_iterator it = set.begin(); it != set.end(); ++it) {
    TabContents* iter_tab = *it;
    if ((iter_tab != tab) &&
        (iter_tab->web_contents()->GetRenderProcessHost() == rph)) {
      return true;
    }
  }
  return false;
}

BackgroundPrintingManager::TabContentsSet::const_iterator
    BackgroundPrintingManager::begin() {
  return printing_tabs_.begin();
}

BackgroundPrintingManager::TabContentsSet::const_iterator
    BackgroundPrintingManager::end() {
  return printing_tabs_.end();
}

bool BackgroundPrintingManager::HasPrintPreviewTab(
    TabContents* preview_tab) {
  if (printing_tabs_.find(preview_tab) != printing_tabs_.end())
    return true;
  return printing_tabs_pending_deletion_.find(preview_tab) !=
      printing_tabs_pending_deletion_.end();
}

}  // namespace printing
