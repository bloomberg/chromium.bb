// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/print_view_manager_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/print_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "printing/printed_document.h"
#include "ui/base/l10n/l10n_util.h"

using base::TimeDelta;
using content::BrowserThread;

namespace {

// Release the PrinterQuery identified by |cookie|.
void ReleasePrinterQuery(int cookie) {
  printing::PrintJobManager* print_job_manager =
      g_browser_process->print_job_manager();
  // May be NULL in tests.
  if (!print_job_manager)
    return;

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager->PopPrinterQuery(cookie, &printer_query);
  if (printer_query.get()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&printing::PrinterQuery::StopWorker, printer_query.get()));
  }
}

// Keeps track of pending scripted print preview closures.
// No locking, only access on the UI thread.
typedef std::map<content::RenderProcessHost*, base::Closure>
    ScriptedPrintPreviewClosureMap;
static base::LazyInstance<ScriptedPrintPreviewClosureMap>
    g_scripted_print_preview_closure_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace printing {

PrintViewManager::PrintViewManager(TabContentsWrapper* tab)
    : TabContentsObserver(tab->tab_contents()),
      tab_(tab),
      number_pages_(0),
      printing_succeeded_(false),
      inside_inner_message_loop_(false),
      observer_(NULL),
      cookie_(0),
      print_preview_state_(NOT_PREVIEWING),
      scripted_print_preview_rph_(NULL) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  expecting_first_page_ = true;
#endif
}

PrintViewManager::~PrintViewManager() {
  DCHECK_EQ(NOT_PREVIEWING, print_preview_state_);
  ReleasePrinterQuery(cookie_);
  DisconnectFromCurrentPrintJob();
}

bool PrintViewManager::PrintNow() {
  return PrintNowInternal(new PrintMsg_PrintPages(routing_id()));
}

bool PrintViewManager::PrintForSystemDialogNow() {
  return PrintNowInternal(new PrintMsg_PrintForSystemDialog(routing_id()));
}

bool PrintViewManager::AdvancedPrintNow() {
  PrintPreviewTabController* tab_controller =
      PrintPreviewTabController::GetInstance();
  if (!tab_controller)
    return false;
  TabContentsWrapper* print_preview_tab =
      tab_controller->GetPrintPreviewForTab(tab_);
  if (print_preview_tab) {
    // Preview tab exist for current tab or current tab is preview tab.
    if (!print_preview_tab->web_ui())
      return false;
    PrintPreviewUI* print_preview_ui =
        static_cast<PrintPreviewUI*>(print_preview_tab->web_ui());
    print_preview_ui->OnShowSystemDialog();
    return true;
  } else {
    return PrintNow();
  }
}

bool PrintViewManager::PrintPreviewNow() {
  if (print_preview_state_ != NOT_PREVIEWING) {
    NOTREACHED();
    return false;
  }
  print_preview_state_ = USER_INITIATED_PREVIEW;
  return PrintNowInternal(new PrintMsg_InitiatePrintPreview(routing_id()));
}

void PrintViewManager::PrintPreviewDone() {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  DCHECK_NE(NOT_PREVIEWING, print_preview_state_);

  if (print_preview_state_ == SCRIPTED_PREVIEW) {
    ScriptedPrintPreviewClosureMap& map =
        g_scripted_print_preview_closure_map.Get();
    ScriptedPrintPreviewClosureMap::iterator it =
        map.find(scripted_print_preview_rph_);
    CHECK(it != map.end());
    it->second.Run();
    map.erase(scripted_print_preview_rph_);
    scripted_print_preview_rph_ = NULL;
  }
  print_preview_state_ = NOT_PREVIEWING;
}

void PrintViewManager::PreviewPrintingRequestCancelled() {
  if (!tab_contents())
    return;
  RenderViewHost* rvh = tab_contents()->render_view_host();
  rvh->Send(new PrintMsg_PreviewPrintingRequestCancelled(rvh->routing_id()));
}

void PrintViewManager::set_observer(PrintViewManagerObserver* observer) {
  DCHECK(!observer || !observer_);
  observer_ = observer;
}

void PrintViewManager::StopNavigation() {
  // Cancel the current job, wait for the worker to finish.
  TerminatePrintJob(true);
}

void PrintViewManager::RenderViewGone(base::TerminationStatus status) {
  if (!print_job_.get())
    return;

  scoped_refptr<PrintedDocument> document(print_job_->document());
  if (document) {
    // If IsComplete() returns false, the document isn't completely rendered.
    // Since our renderer is gone, there's nothing to do, cancel it. Otherwise,
    // the print job may finish without problem.
    TerminatePrintJob(!document->IsComplete());
  }
}

string16 PrintViewManager::RenderSourceName() {
  string16 name(tab_contents()->GetTitle());
  if (name.empty())
    name = l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE);
  return name;
}

void PrintViewManager::OnDidGetPrintedPagesCount(int cookie, int number_pages) {
  DCHECK_GT(cookie, 0);
  DCHECK_GT(number_pages, 0);
  number_pages_ = number_pages;
  OpportunisticallyCreatePrintJob(cookie);
}

void PrintViewManager::OnDidGetDocumentCookie(int cookie) {
  cookie_ = cookie;
}

void PrintViewManager::OnDidShowPrintDialog() {
  if (observer_)
    observer_->OnPrintDialogShown();
}

void PrintViewManager::OnDidPrintPage(
    const PrintHostMsg_DidPrintPage_Params& params) {
  if (!OpportunisticallyCreatePrintJob(params.document_cookie))
    return;

  PrintedDocument* document = print_job_->document();
  if (!document || params.document_cookie != document->cookie()) {
    // Out of sync. It may happen since we are completely asynchronous. Old
    // spurious messages can be received if one of the processes is overloaded.
    return;
  }

#if defined(OS_WIN)
  // http://msdn2.microsoft.com/en-us/library/ms535522.aspx
  // Windows 2000/XP: When a page in a spooled file exceeds approximately 350
  // MB, it can fail to print and not send an error message.
  if (params.data_size && params.data_size >= 350*1024*1024) {
    NOTREACHED() << "size:" << params.data_size;
    TerminatePrintJob(true);
    tab_contents()->Stop();
    return;
  }
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
  const bool metafile_must_be_valid = true;
#elif defined(OS_POSIX)
  const bool metafile_must_be_valid = expecting_first_page_;
  expecting_first_page_ = false;
#endif

  base::SharedMemory shared_buf(params.metafile_data_handle, true);
  if (metafile_must_be_valid) {
    if (!shared_buf.Map(params.data_size)) {
      NOTREACHED() << "couldn't map";
      tab_contents()->Stop();
      return;
    }
  }

  scoped_ptr<Metafile> metafile(new NativeMetafile);
  if (metafile_must_be_valid) {
    if (!metafile->InitFromData(shared_buf.memory(), params.data_size)) {
      NOTREACHED() << "Invalid metafile header";
      tab_contents()->Stop();
      return;
    }
  }

  // Update the rendered document. It will send notifications to the listener.
  document->SetPage(params.page_number,
                    metafile.release(),
                    params.actual_shrink,
                    params.page_size,
                    params.content_area);

  ShouldQuitFromInnerMessageLoop();
}

void PrintViewManager::OnPrintingFailed(int cookie) {
  ReleasePrinterQuery(cookie);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_RELEASED,
      content::Source<TabContents>(tab_contents()),
      content::NotificationService::NoDetails());
}

void PrintViewManager::OnScriptedPrintPreview(IPC::Message* reply_msg) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  ScriptedPrintPreviewClosureMap& map =
      g_scripted_print_preview_closure_map.Get();
  content::RenderProcessHost* rph =
      tab_contents()->render_view_host()->process();

  // This should always be 0 once we get modal window.print().
  if (map.count(rph) != 0) {
    // Renderer already handling window.print() in another View.
    Send(reply_msg);
    return;
  }
  if (print_preview_state_ != NOT_PREVIEWING) {
    // If a user initiated print dialog is already open, ignore the scripted
    // print message.
    DCHECK_EQ(USER_INITIATED_PREVIEW, print_preview_state_);
    Send(reply_msg);
    return;
  }

  print_preview_state_ = SCRIPTED_PREVIEW;
  base::Closure callback =
      base::Bind(&PrintViewManager::OnScriptedPrintPreviewReply,
                 base::Unretained(this),
                 reply_msg);
  map[rph] = callback;
  scripted_print_preview_rph_ = rph;

  PrintPreviewTabController::PrintPreview(tab_);
}

void PrintViewManager::OnScriptedPrintPreviewReply(IPC::Message* reply_msg) {
  BrowserThread::CurrentlyOn(BrowserThread::UI);
  Send(reply_msg);
}

bool PrintViewManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintViewManager, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetPrintedPagesCount,
                        OnDidGetPrintedPagesCount)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDocumentCookie,
                        OnDidGetDocumentCookie)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidShowPrintDialog, OnDidShowPrintDialog)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintPage, OnDidPrintPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintingFailed, OnPrintingFailed)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PrintHostMsg_ScriptedPrintPreview,
                                    OnScriptedPrintPreview)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PrintViewManager::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PRINT_JOB_EVENT: {
      OnNotifyPrintJobEvent(*content::Details<JobEventDetails>(details).ptr());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintViewManager::OnNotifyPrintJobEvent(
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      TerminatePrintJob(true);

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<TabContentsWrapper>(tab_),
          content::NotificationService::NoDetails());
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      NOTREACHED();
      break;
    }
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      ShouldQuitFromInnerMessageLoop();
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::NEW_PAGE:
    case JobEventDetails::PAGE_DONE:
    case JobEventDetails::DOC_DONE: {
      // Don't care about the actual printing process.
      break;
    }
    case JobEventDetails::JOB_DONE: {
      // Printing is done, we don't need it anymore.
      // print_job_->is_job_pending() may still be true, depending on the order
      // of object registration.
      printing_succeeded_ = true;
      ReleasePrintJob();

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<TabContentsWrapper>(tab_),
          content::NotificationService::NoDetails());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

bool PrintViewManager::RenderAllMissingPagesNow() {
  if (!print_job_.get() || !print_job_->is_job_pending())
    return false;

  // We can't print if there is no renderer.
  if (!tab_contents() ||
      !tab_contents()->render_view_host() ||
      !tab_contents()->render_view_host()->IsRenderViewLive()) {
    return false;
  }

  // Is the document already complete?
  if (print_job_->document() && print_job_->document()->IsComplete()) {
    printing_succeeded_ = true;
    return true;
  }

  // TabContents is either dying or a second consecutive request to print
  // happened before the first had time to finish. We need to render all the
  // pages in an hurry if a print_job_ is still pending. No need to wait for it
  // to actually spool the pages, only to have the renderer generate them. Run
  // a message loop until we get our signal that the print job is satisfied.
  // PrintJob will send a ALL_PAGES_REQUESTED after having received all the
  // pages it needs. MessageLoop::current()->Quit() will be called as soon as
  // print_job_->document()->IsComplete() is true on either ALL_PAGES_REQUESTED
  // or in DidPrintPage(). The check is done in
  // ShouldQuitFromInnerMessageLoop().
  // BLOCKS until all the pages are received. (Need to enable recursive task)
  if (!RunInnerMessageLoop()) {
    // This function is always called from DisconnectFromCurrentPrintJob() so we
    // know that the job will be stopped/canceled in any case.
    return false;
  }
  return true;
}

void PrintViewManager::ShouldQuitFromInnerMessageLoop() {
  // Look at the reason.
  DCHECK(print_job_->document());
  if (print_job_->document() &&
      print_job_->document()->IsComplete() &&
      inside_inner_message_loop_) {
    // We are in a message loop created by RenderAllMissingPagesNow. Quit from
    // it.
    MessageLoop::current()->Quit();
    inside_inner_message_loop_ = false;
  }
}

bool PrintViewManager::CreateNewPrintJob(PrintJobWorkerOwner* job) {
  DCHECK(!inside_inner_message_loop_);

  // Disconnect the current print_job_.
  DisconnectFromCurrentPrintJob();

  // We can't print if there is no renderer.
  if (!tab_contents()->render_view_host() ||
      !tab_contents()->render_view_host()->IsRenderViewLive()) {
    return false;
  }

  // Ask the renderer to generate the print preview, create the print preview
  // view and switch to it, initialize the printer and show the print dialog.
  DCHECK(!print_job_.get());
  DCHECK(job);
  if (!job)
    return false;

  print_job_ = new PrintJob();
  print_job_->Initialize(job, this, number_pages_);
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::Source<PrintJob>(print_job_.get()));
  printing_succeeded_ = false;
  return true;
}

void PrintViewManager::DisconnectFromCurrentPrintJob() {
  // Make sure all the necessary rendered page are done. Don't bother with the
  // return value.
  bool result = RenderAllMissingPagesNow();

  // Verify that assertion.
  if (print_job_.get() &&
      print_job_->document() &&
      !print_job_->document()->IsComplete()) {
    DCHECK(!result);
    // That failed.
    TerminatePrintJob(true);
  } else {
    // DO NOT wait for the job to finish.
    ReleasePrintJob();
  }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  expecting_first_page_ = true;
#endif
}

void PrintViewManager::PrintingDone(bool success) {
  if (!print_job_.get() || !tab_contents())
    return;
  RenderViewHost* rvh = tab_contents()->render_view_host();
  rvh->Send(new PrintMsg_PrintingDone(rvh->routing_id(), success));
}

void PrintViewManager::TerminatePrintJob(bool cancel) {
  if (!print_job_.get())
    return;

  if (cancel) {
    // We don't need the metafile data anymore because the printing is canceled.
    print_job_->Cancel();
    inside_inner_message_loop_ = false;
  } else {
    DCHECK(!inside_inner_message_loop_);
    DCHECK(!print_job_->document() || print_job_->document()->IsComplete());

    // TabContents is either dying or navigating elsewhere. We need to render
    // all the pages in an hurry if a print job is still pending. This does the
    // trick since it runs a blocking message loop:
    print_job_->Stop();
  }
  ReleasePrintJob();
}

void PrintViewManager::ReleasePrintJob() {
  if (!print_job_.get())
    return;

  PrintingDone(printing_succeeded_);

  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                    content::Source<PrintJob>(print_job_.get()));
  print_job_->DisconnectSource();
  // Don't close the worker thread.
  print_job_ = NULL;
}

bool PrintViewManager::RunInnerMessageLoop() {
  // This value may actually be too low:
  //
  // - If we're looping because of printer settings initialization, the premise
  // here is that some poor users have their print server away on a VPN over a
  // slow connection. In this situation, the simple fact of opening the printer
  // can be dead slow. On the other side, we don't want to die infinitely for a
  // real network error. Give the printer 60 seconds to comply.
  //
  // - If we're looping because of renderer page generation, the renderer could
  // be CPU bound, the page overly complex/large or the system just
  // memory-bound.
  static const int kPrinterSettingsTimeout = 60000;
  base::OneShotTimer<MessageLoop> quit_timer;
  quit_timer.Start(FROM_HERE,
                   TimeDelta::FromMilliseconds(kPrinterSettingsTimeout),
                   MessageLoop::current(), &MessageLoop::Quit);

  inside_inner_message_loop_ = true;

  // Need to enable recursive task.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  // Restore task state.
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  bool success = true;
  if (inside_inner_message_loop_) {
    // Ok we timed out. That's sad.
    inside_inner_message_loop_ = false;
    success = false;
  }

  return success;
}

bool PrintViewManager::OpportunisticallyCreatePrintJob(int cookie) {
  if (print_job_.get())
    return true;

  if (!cookie) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return false;
  }

  // The job was initiated by a script. Time to get the corresponding worker
  // thread.
  scoped_refptr<PrinterQuery> queued_query;
  g_browser_process->print_job_manager()->PopPrinterQuery(cookie,
                                                          &queued_query);
  DCHECK(queued_query.get());
  if (!queued_query.get())
    return false;

  if (!CreateNewPrintJob(queued_query.get())) {
    // Don't kill anything.
    return false;
  }

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

bool PrintViewManager::PrintNowInternal(IPC::Message* message) {
  // Don't print / print preview interstitials.
  if (tab_contents()->showing_interstitial_page())
    return false;
  return Send(message);
}

}  // namespace printing
