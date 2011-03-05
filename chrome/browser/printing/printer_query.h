// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/printing/print_job_worker_owner.h"
#include "ui/gfx/native_widget_types.h"

class CancelableTask;
class DictionaryValue;
class MessageLoop;

namespace base {
class Thread;
}

namespace printing {

class PrintJobWorker;

// Query the printer for settings.
class PrinterQuery : public PrintJobWorkerOwner {
 public:
  // GetSettings() UI parameter.
  enum GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  PrinterQuery();

  // PrintJobWorkerOwner
  virtual void GetSettingsDone(const PrintSettings& new_settings,
                               PrintingContext::Result result);
  virtual PrintJobWorker* DetachWorker(PrintJobWorkerOwner* new_owner);
  virtual MessageLoop* message_loop();
  virtual const PrintSettings& settings() const;
  virtual int cookie() const;

  // Initializes the printing context. It is fine to call this function multiple
  // times to reinitialize the settings. |parent_view| parameter's window will
  // be the owner of the print setting dialog box. It is unused when
  // |ask_for_user_settings| is DEFAULTS.
  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   gfx::NativeView parent_view,
                   int expected_page_count,
                   bool has_selection,
                   bool use_overlays,
                   CancelableTask* callback);

  // Updates the current settings with |new_settings| dictionary values.
  void SetSettings(const DictionaryValue& new_settings,
                   CancelableTask* callback);

  // Stops the worker thread since the client is done with this object.
  void StopWorker();

  // Returns true if a GetSettings() call is pending completion.
  bool is_callback_pending() const;

  PrintingContext::Result last_status() const { return last_status_; }

  // Returns if a worker thread is still associated to this instance.
  bool is_valid() const;

 private:
  virtual ~PrinterQuery();

  // Lazy create the worker thread. There is one worker thread per print job.
  // Returns true, if worker thread exists or has been created.
  bool StartWorker(CancelableTask* callback);

  // Main message loop reference. Used to send notifications in the right
  // thread.
  MessageLoop* const io_message_loop_;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  scoped_ptr<PrintJobWorker> worker_;

  // Cache of the print context settings for access in the UI thread.
  PrintSettings settings_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_;

  // Cookie that make this instance unique.
  int cookie_;

  // Results from the last GetSettingsDone() callback.
  PrintingContext::Result last_status_;

  // Task waiting to be executed.
  scoped_ptr<CancelableTask> callback_;

  DISALLOW_COPY_AND_ASSIGN(PrinterQuery);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
