// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
#define CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/printing/print_job_worker_owner.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class PrintJobWorker;

// Query the printer for settings.
class PrinterQuery : public PrintJobWorkerOwner {
 public:
  // GetSettings() UI parameter.
  enum class GetSettingsAskParam {
    DEFAULTS,
    ASK_USER,
  };

  // Can only be called on the IO thread.
  PrinterQuery(int render_process_id, int render_frame_id);

  // PrintJobWorkerOwner implementation.
  void GetSettingsDone(const PrintSettings& new_settings,
                       PrintingContext::Result result) override;
  std::unique_ptr<PrintJobWorker> DetachWorker(
      PrintJobWorkerOwner* new_owner) override;
  const PrintSettings& settings() const override;
  int cookie() const override;

  // Initializes the printing context. It is fine to call this function multiple
  // times to reinitialize the settings. |web_contents_observer| can be queried
  // to find the owner of the print setting dialog box. It is unused when
  // |ask_for_user_settings| is DEFAULTS.
  void GetSettings(GetSettingsAskParam ask_user_for_settings,
                   int expected_page_count,
                   bool has_selection,
                   MarginType margin_type,
                   bool is_scripted,
                   bool is_modifiable,
                   base::OnceClosure callback);

  // Updates the current settings with |new_settings| dictionary values.
  virtual void SetSettings(std::unique_ptr<base::DictionaryValue> new_settings,
                           base::OnceClosure callback);

#if defined(OS_CHROMEOS)
  // Updates the current settings with |new_settings|.
  void SetSettingsFromPOD(std::unique_ptr<printing::PrintSettings> new_settings,
                          base::OnceClosure callback);
#endif

  // Stops the worker thread since the client is done with this object.
  virtual void StopWorker();

  // Returns true if a GetSettings() call is pending completion.
  bool is_callback_pending() const;

  PrintingContext::Result last_status() const { return last_status_; }

  // Returns if a worker thread is still associated to this instance.
  bool is_valid() const;

 protected:
  // For unit tests to manually set the print callback.
  void set_callback(base::OnceClosure callback);

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  std::unique_ptr<PrintJobWorker> worker_;

  // Refcounted class.
  ~PrinterQuery() override;

 private:
  // Lazy create the worker thread. There is one worker thread per print job.
  void StartWorker(base::OnceClosure callback);

  // Cache of the print context settings for access in the UI thread.
  PrintSettings settings_;

  // Is the Print... dialog box currently shown.
  bool is_print_dialog_box_shown_ = false;

  // Cookie that make this instance unique.
  int cookie_;

  // Results from the last GetSettingsDone() callback.
  PrintingContext::Result last_status_ = PrintingContext::FAILED;

  // Callback waiting to be run.
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(PrinterQuery);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINTER_QUERY_H_
