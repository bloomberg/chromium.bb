// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

namespace printing {

PrinterQuery::PrinterQuery(int render_process_id, int render_frame_id)
    : base::RefCountedDeleteOnSequence<PrinterQuery>(
          base::ThreadTaskRunnerHandle::Get()),
      cookie_(PrintSettings::NewCookie()),
      worker_(std::make_unique<PrintJobWorker>(render_process_id,
                                               render_frame_id,
                                               this)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

PrinterQuery::~PrinterQuery() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_print_dialog_box_shown_);
  // If this fires, it is that this pending printer context has leaked.
  DCHECK(!worker_);
}

void PrinterQuery::GetSettingsDone(const PrintSettings& new_settings,
                                   PrintingContext::Result result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  is_print_dialog_box_shown_ = false;
  last_status_ = result;
  if (result != PrintingContext::FAILED) {
    settings_ = new_settings;
    cookie_ = PrintSettings::NewCookie();
  } else {
    // Failure.
    cookie_ = 0;
  }

  if (callback_) {
    // This may cause reentrancy like to call StopWorker().
    std::move(callback_).Run();
  }
}

std::unique_ptr<PrintJobWorker> PrinterQuery::DetachWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback_);
  DCHECK(worker_);

  return std::move(worker_);
}

const PrintSettings& PrinterQuery::settings() const {
  return settings_;
}

int PrinterQuery::cookie() const {
  return cookie_;
}

void PrinterQuery::set_callback(base::OnceClosure callback) {
  callback_ = std::move(callback);
}

void PrinterQuery::GetSettings(GetSettingsAskParam ask_user_for_settings,
                               int expected_page_count,
                               bool has_selection,
                               MarginType margin_type,
                               bool is_scripted,
                               bool is_modifiable,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!is_print_dialog_box_shown_ || !is_scripted);

  StartWorker(std::move(callback));

  // Real work is done in PrintJobWorker::GetSettings().
  is_print_dialog_box_shown_ =
      ask_user_for_settings == GetSettingsAskParam::ASK_USER;
  worker_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorker::GetSettings,
                     base::Unretained(worker_.get()),
                     is_print_dialog_box_shown_, expected_page_count,
                     has_selection, margin_type, is_scripted, is_modifiable));
}

void PrinterQuery::SetSettings(base::Value new_settings,
                               base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  StartWorker(std::move(callback));
  worker_->PostTask(FROM_HERE, base::BindOnce(&PrintJobWorker::SetSettings,
                                              base::Unretained(worker_.get()),
                                              std::move(new_settings)));
}

#if defined(OS_CHROMEOS)
void PrinterQuery::SetSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  StartWorker(std::move(callback));
  worker_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorker::SetSettingsFromPOD,
                     base::Unretained(worker_.get()), std::move(new_settings)));
}
#endif

void PrinterQuery::StartWorker(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!callback_);
  DCHECK(worker_);

  // Lazily create the worker thread. There is one worker thread per print job.
  if (!worker_->IsRunning())
    worker_->Start();

  callback_ = std::move(callback);
}

void PrinterQuery::StopWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (worker_) {
    // http://crbug.com/66082: We're blocking on the PrinterQuery's worker
    // thread.  It's not clear to me if this may result in blocking the current
    // thread for an unacceptable time.  We should probably fix it.
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
    worker_->Stop();
    worker_.reset();
  }
}

bool PrinterQuery::PostTask(const base::Location& from_here,
                            base::OnceClosure task) {
  return base::PostTaskWithTraits(from_here, {content::BrowserThread::IO},
                                  std::move(task));
}

bool PrinterQuery::is_callback_pending() const {
  return !callback_.is_null();
}

bool PrinterQuery::is_valid() const {
  return !!worker_;
}

}  // namespace printing
