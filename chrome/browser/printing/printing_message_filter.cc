// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printing_message_filter.h"

#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

#if defined(OS_CHROMEOS)
#include <fcntl.h>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#else
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
typedef std::map<int, FilePath> SequenceToPathMap;

struct PrintingSequencePathMap {
  SequenceToPathMap map;
  int sequence;
};

// No locking, only access on the FILE thread.
static base::LazyInstance<PrintingSequencePathMap>
    g_printing_file_descriptor_map(base::LINKER_INITIALIZED);
#endif

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   ViewMsg_Print_Params* params) {
  params->page_size = settings.page_setup_device_units().physical_size();
  params->printable_size.SetSize(
      settings.page_setup_device_units().content_area().width(),
      settings.page_setup_device_units().content_area().height());
  params->margin_top = settings.page_setup_device_units().content_area().x();
  params->margin_left = settings.page_setup_device_units().content_area().y();
  params->dpi = settings.dpi();
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = settings.min_shrink;
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = settings.max_shrink;
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = settings.desired_dpi;
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only;
  params->supports_alpha_blend = settings.supports_alpha_blend();
}

}  // namespace

PrintingMessageFilter::PrintingMessageFilter()
    : print_job_manager_(g_browser_process->print_job_manager()) {
#if defined(OS_CHROMEOS)
  cloud_print_enabled_ = true;
#else
  cloud_print_enabled_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrint);
#endif
}

PrintingMessageFilter::~PrintingMessageFilter() {
}

void PrintingMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
#if defined(OS_CHROMEOS)
  if (message.type() == ViewHostMsg_AllocateTempFileForPrinting::ID ||
      message.type() == ViewHostMsg_TempFileForPrintingWritten::ID) {
    *thread = BrowserThread::FILE;
  }
#endif
}

bool PrintingMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PrintingMessageFilter, message, *message_was_ok)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateTempFileForPrinting,
                        OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint, OnScriptedPrint)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_UpdatePrintSettings,
                                    OnUpdatePrintSettings)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if defined(OS_WIN)
void PrintingMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, peer_handle());
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}
#endif

#if defined(OS_CHROMEOS)
void PrintingMessageFilter::OnAllocateTempFileForPrinting(
    base::FileDescriptor* temp_file_fd, int* sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  temp_file_fd->fd = *sequence_number = -1;
  temp_file_fd->auto_close = false;

  SequenceToPathMap* map = &g_printing_file_descriptor_map.Get().map;
  *sequence_number = g_printing_file_descriptor_map.Get().sequence++;

  FilePath path;
  if (file_util::CreateTemporaryFile(&path)) {
    int fd = open(path.value().c_str(), O_WRONLY);
    if (fd >= 0) {
      SequenceToPathMap::iterator it = map->find(*sequence_number);
      if (it != map->end()) {
        NOTREACHED() << "Sequence number already in use. seq=" <<
            *sequence_number;
      } else {
        (*map)[*sequence_number] = path;
        temp_file_fd->fd = fd;
        temp_file_fd->auto_close = true;
      }
    }
  }
}

void PrintingMessageFilter::OnTempFileForPrintingWritten(int sequence_number) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SequenceToPathMap* map = &g_printing_file_descriptor_map.Get().map;
  SequenceToPathMap::iterator it = map->find(sequence_number);
  if (it == map->end()) {
    NOTREACHED() << "Got a sequence that we didn't pass to the "
                    "renderer: " << sequence_number;
    return;
  }

  if (cloud_print_enabled_)
    PrintDialogCloud::CreatePrintDialogForPdf(it->second, string16(), true);
  else
    NOTIMPLEMENTED();

  // Erase the entry in the map.
  map->erase(it);
}
#endif  // defined(OS_CHROMEOS)

void PrintingMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  if (!print_job_manager_->printing_enabled()) {
    // Reply with NULL query.
    OnGetDefaultPrintSettingsReply(printer_query, reply_msg);
    return;
  }

  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &PrintingMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             false,
                             true,
                             task);
}

void PrintingMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If printing was enabled.
  if (printer_query.get()) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      print_job_manager_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void PrintingMessageFilter::OnScriptedPrint(
    const ViewHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  gfx::NativeView host_view =
      gfx::NativeViewFromIdInBrowser(params.host_window_id);

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(params.cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &PrintingMessageFilter::OnScriptedPrintReply,
      printer_query,
      params.routing_id,
      reply_msg);

  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_view,
                             params.expected_pages_count,
                             params.has_selection,
                             params.use_overlays,
                             task);
}

void PrintingMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    int routing_id,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages =
        printing::PageRange::GetPages(printer_query->settings().ranges);
  }
  ViewHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

void PrintingMessageFilter::OnUpdatePrintSettings(
    int document_cookie, const DictionaryValue& job_settings,
    IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(document_cookie, &printer_query);
  if (printer_query.get()) {
    CancelableTask* task = NewRunnableMethod(
        this,
        &PrintingMessageFilter::OnUpdatePrintSettingsReply,
        printer_query,
        reply_msg);
    printer_query->SetSettings(job_settings, task);
  }
}

void PrintingMessageFilter::OnUpdatePrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_UpdatePrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query->cookie() && printer_query->settings().dpi())
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  else
    printer_query->StopWorker();
}
