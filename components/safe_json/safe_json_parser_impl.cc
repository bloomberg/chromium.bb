// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_json/safe_json_parser_impl.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/tuple.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/common/service_registry.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace safe_json {

SafeJsonParserImpl::SafeJsonParserImpl(const std::string& unsafe_json,
                                       const SuccessCallback& success_callback,
                                       const ErrorCallback& error_callback)
    : unsafe_json_(unsafe_json),
      success_callback_(success_callback),
      error_callback_(error_callback) {
  io_thread_checker_.DetachFromThread();
}

SafeJsonParserImpl::~SafeJsonParserImpl() {
}

void SafeJsonParserImpl::StartWorkOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(io_thread_checker_.CalledOnValidThread());
  // TODO(amistry): This UtilityProcessHost dance is likely to be done multiple
  // times as more tasks are migrated to Mojo. Create some sort of helper class
  // to eliminate the code duplication.
  utility_process_host_ = UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get().get())->AsWeakPtr();
  utility_process_host_->SetName(
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_JSON_PARSER_NAME));
  if (!utility_process_host_->Start()) {
    ReportResults();
    return;
  }

  content::ServiceRegistry* service_registry =
      utility_process_host_->GetServiceRegistry();
  service_registry->ConnectToRemoteService(mojo::GetProxy(&service_));
  service_.set_connection_error_handler(
      base::Bind(&SafeJsonParserImpl::ReportResults, this));

  service_->Parse(unsafe_json_,
                  base::Bind(&SafeJsonParserImpl::OnParseDone, this));
}

void SafeJsonParserImpl::ReportResults() {
  // There should be a DCHECK_CURRENTLY_ON(BrowserThread::IO) here. However, if
  // the parser process is still alive on shutdown, this might run on the IO
  // thread while the IO thread message loop is shutting down. This happens
  // after the IO thread has unregistered from the BrowserThread list, causing
  // the DCHECK to fail.
  DCHECK(io_thread_checker_.CalledOnValidThread());
  io_thread_checker_.DetachFromThread();

  // Maintain a reference to |this| since either |utility_process_host_| or
  // |service_| may have the last reference and destroying those might delete
  // |this|.
  scoped_refptr<SafeJsonParserImpl> ref(this);
  // Shut down the utility process if it's still running.
  delete utility_process_host_.get();
  service_.reset();

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SafeJsonParserImpl::ReportResultsOnOriginThread, this));
}

void SafeJsonParserImpl::ReportResultsOnOriginThread() {
  DCHECK(caller_task_runner_->RunsTasksOnCurrentThread());
  if (error_.empty() && parsed_json_) {
    if (!success_callback_.is_null())
      success_callback_.Run(std::move(parsed_json_));
  } else {
    if (!error_callback_.is_null())
      error_callback_.Run(error_);
  }
}

bool SafeJsonParserImpl::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void SafeJsonParserImpl::OnParseDone(const base::ListValue& wrapper,
                                     mojo::String error) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (!wrapper.empty()) {
    const base::Value* value = NULL;
    CHECK(wrapper.Get(0, &value));
    parsed_json_.reset(value->DeepCopy());
  } else {
    error_ = error.get();
  }
  ReportResults();
}

void SafeJsonParserImpl::Start() {
  caller_task_runner_ = base::SequencedTaskRunnerHandle::Get();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeJsonParserImpl::StartWorkOnIOThread, this));
}

}  // namespace safe_json
