// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
#pragma once

#include <vector>

#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

namespace printing {

class JobEventDetails;
class PrintedDocument;
class PrintJob;
class PrintedPage;
class PrinterQuery;

class PrintJobManager : public NotificationObserver {
 public:
  PrintJobManager();
  ~PrintJobManager();

  // On browser quit, we should wait to have the print job finished.
  void OnQuit();

  // Stops all printing jobs. If wait_for_finish is true, tries to give jobs
  // a chance to complete before stopping them.
  void StopJobs(bool wait_for_finish);

  // Queues a semi-initialized worker thread. Can be called from any thread.
  // Current use case is queuing from the I/O thread.
  // TODO(maruel):  Have them vanish after a timeout (~5 minutes?)
  void QueuePrinterQuery(PrinterQuery* job);

  // Pops a queued PrintJobWorkerOwner object that was previously queued. Can be
  // called from any thread. Current use case is poping from the browser thread.
  void PopPrinterQuery(int document_cookie, scoped_refptr<PrinterQuery>* job);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  bool printing_enabled();

  void set_printing_enabled(bool printing_enabled);

 private:
  typedef std::vector<scoped_refptr<PrintJob> > PrintJobs;
  typedef std::vector<scoped_refptr<PrinterQuery> > PrinterQueries;

  // Processes a NOTIFY_PRINT_JOB_EVENT notification.
  void OnPrintJobEvent(PrintJob* print_job,
                       const JobEventDetails& event_details);

  NotificationRegistrar registrar_;

  // Used to serialize access to queued_workers_.
  base::Lock lock_;

  // Used to serialize access to printing_enabled_
  base::Lock enabled_lock_;

  PrinterQueries queued_queries_;

  // Current print jobs that are active.
  PrintJobs current_jobs_;

  // Printing is enabled/disabled. This variable is checked at only one place,
  // by RenderMessageFilter::OnGetDefaultPrintSettings. If its value is true
  // at that point, then the initiated print flow will complete itself,
  // even if the value of this variable changes afterwards.
  bool printing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PrintJobManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_MANAGER_H_
