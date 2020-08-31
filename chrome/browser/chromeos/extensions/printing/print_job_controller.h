// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace chromeos {
class CupsPrintJob;
class CupsPrintJobManager;
}  // namespace chromeos

namespace printing {
class MetafileSkia;
class PrintSettings;
}  // namespace printing

namespace extensions {

// This class is responsible for sending print jobs in the printing pipeline and
// cancelling them. It should be used by API handler as the entry point of
// actual printing pipeline.
class PrintJobController {
 public:
  using StartPrintJobCallback =
      base::OnceCallback<void(std::unique_ptr<std::string> job_id)>;

  static std::unique_ptr<PrintJobController> Create(
      chromeos::CupsPrintJobManager* print_job_manager);

  PrintJobController() = default;
  virtual ~PrintJobController() = default;
  PrintJobController(const PrintJobController&) = delete;
  PrintJobController& operator=(const PrintJobController&) = delete;

  // Creates, initializes and adds print job to the queue of pending print jobs.
  virtual void StartPrintJob(const std::string& extension_id,
                             std::unique_ptr<printing::MetafileSkia> metafile,
                             std::unique_ptr<printing::PrintSettings> settings,
                             StartPrintJobCallback callback) = 0;

  // Returns false if there is no active print job with specified id.
  // Returns true otherwise.
  virtual bool CancelPrintJob(const std::string& job_id) = 0;

  // This should be called when CupsPrintJobManager created CupsPrintJob.
  virtual void OnPrintJobCreated(
      const std::string& extension_id,
      const std::string& job_id,
      base::WeakPtr<chromeos::CupsPrintJob> cups_job) = 0;

  // This should be called when CupsPrintJob is finished (it could be either
  // completed, failed or cancelled).
  virtual void OnPrintJobFinished(const std::string& job_id) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_PRINT_JOB_CONTROLLER_H_
