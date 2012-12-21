// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/background_printing_manager.h"

#include "base/stl_util.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
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
  // The might be some WebContentses still in |printing_contents_set_| at this
  // point. E.g. when the last remaining tab closes and there is still a print
  // preview WebContents trying to print. In which case it will fail to print.
  // TODO(thestig): Handle this case better.
}

void BackgroundPrintingManager::OwnPrintPreviewDialog(
    WebContents* preview_dialog) {
  DCHECK(CalledOnValidThread());
  DCHECK(PrintPreviewDialogController::IsPrintPreviewDialog(preview_dialog));
  CHECK(!HasPrintPreviewDialog(preview_dialog));

  printing_contents_set_.insert(preview_dialog);

  content::Source<WebContents> preview_source(preview_dialog);
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED, preview_source);

  // OwnInitiatorWebContents() may have already added this notification.
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED, preview_source)) {
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   preview_source);
  }

  // If a WebContents that is printing crashes, the user cannot destroy it since
  // it is hidden. Thus listen for crashes here and delete it.
  //
  // Multiple sites may share the same RenderProcessHost, so check if this
  // notification has already been added.
  content::Source<content::RenderProcessHost> rph_source(
      preview_dialog->GetRenderProcessHost());
  if (!registrar_.IsRegistered(this,
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED, rph_source)) {
    registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                   rph_source);
  }

  // Activate the initiator tab.
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return;
  WebContents* initiator_tab =
      dialog_controller->GetInitiatorTab(preview_dialog);
  if (!initiator_tab)
    return;
  initiator_tab->GetDelegate()->ActivateContents(initiator_tab);
}

void BackgroundPrintingManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_RENDERER_PROCESS_CLOSED) {
    OnRendererProcessClosed(
        content::Source<content::RenderProcessHost>(source).ptr());
  } else if (type == chrome::NOTIFICATION_PRINT_JOB_RELEASED) {
    OnPrintJobReleased(content::Source<WebContents>(source).ptr());
  } else {
    DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);
    OnWebContentsDestroyed(content::Source<WebContents>(source).ptr());
  }
}

void BackgroundPrintingManager::OnRendererProcessClosed(
    content::RenderProcessHost* rph) {
  WebContentsSet printing_contents_pending_deletion_set;
  WebContentsSet::const_iterator it;
  for (it = begin(); it != end(); ++it) {
    WebContents* preview_contents = *it;
    if (preview_contents->GetRenderProcessHost() == rph) {
      printing_contents_pending_deletion_set.insert(preview_contents);
    }
  }
  for (it = printing_contents_pending_deletion_set.begin();
       it != printing_contents_pending_deletion_set.end();
       ++it) {
    DeletePreviewContents(*it);
  }
}

void BackgroundPrintingManager::OnPrintJobReleased(
    WebContents* preview_contents) {
  DeletePreviewContents(preview_contents);
}

void BackgroundPrintingManager::OnWebContentsDestroyed(
    WebContents* preview_contents) {
  // Always need to remove this notification since the WebContents is gone.
  content::Source<WebContents> preview_source(preview_contents);
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    preview_source);

  if (!HasPrintPreviewDialog(preview_contents)) {
    NOTREACHED();
    return;
  }

  // Remove NOTIFICATION_RENDERER_PROCESS_CLOSED if |preview_contents| is the
  // last WebContents associated with |rph|.
  bool shared_rph =
      (HasSharedRenderProcessHost(printing_contents_set_, preview_contents) ||
       HasSharedRenderProcessHost(printing_contents_pending_deletion_set_,
                                  preview_contents));
  if (!shared_rph) {
    content::RenderProcessHost* rph = preview_contents->GetRenderProcessHost();
    registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                      content::Source<content::RenderProcessHost>(rph));
  }

  // Remove other notifications and remove the WebContents from its
  // WebContentsSet.
  if (printing_contents_set_.erase(preview_contents) == 1) {
    registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                      preview_source);
  } else {
    // DeletePreviewContents already deleted the notification.
    printing_contents_pending_deletion_set_.erase(preview_contents);
  }
}

void BackgroundPrintingManager::DeletePreviewContents(
    WebContents* preview_contents) {
  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_RELEASED,
                    content::Source<WebContents>(preview_contents));
  printing_contents_set_.erase(preview_contents);
  printing_contents_pending_deletion_set_.insert(preview_contents);
  MessageLoop::current()->DeleteSoon(FROM_HERE, preview_contents);
}

bool BackgroundPrintingManager::HasSharedRenderProcessHost(
    const WebContentsSet& set, WebContents* preview_contents) {
  content::RenderProcessHost* rph = preview_contents->GetRenderProcessHost();
  for (WebContentsSet::const_iterator it = set.begin(); it != set.end(); ++it) {
    WebContents* iter_contents = *it;
    if (iter_contents == preview_contents)
      continue;
    if (iter_contents->GetRenderProcessHost() == rph)
      return true;
  }
  return false;
}

BackgroundPrintingManager::WebContentsSet::const_iterator
    BackgroundPrintingManager::begin() {
  return printing_contents_set_.begin();
}

BackgroundPrintingManager::WebContentsSet::const_iterator
    BackgroundPrintingManager::end() {
  return printing_contents_set_.end();
}

bool BackgroundPrintingManager::HasPrintPreviewDialog(
    WebContents* preview_dialog) {
  return (ContainsKey(printing_contents_set_, preview_dialog) ||
          ContainsKey(printing_contents_pending_deletion_set_, preview_dialog));
}

}  // namespace printing
