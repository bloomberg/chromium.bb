// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_feedback_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/supports_user_data.h"
#include "base/task_runner.h"
#include "chrome/browser/safe_browsing/download_feedback.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"

namespace safe_browsing {

namespace {

const void* const kPingKey = &kPingKey;

class DownloadFeedbackPings : public base::SupportsUserData::Data {
 public:
  DownloadFeedbackPings(const std::string& ping_request,
                        const std::string& ping_response);

  // Stores the ping data in the given |download|.
  static void CreateForDownload(content::DownloadItem* download,
                                const std::string& ping_request,
                                const std::string& ping_response);

  // Returns the DownloadFeedbackPings object associated with |download|.  May
  // return NULL.
  static DownloadFeedbackPings* FromDownload(
      const content::DownloadItem& download);


  const std::string& ping_request() const {
    return ping_request_;
  }

  const std::string& ping_response() const {
    return ping_response_;
  }

 private:
  std::string ping_request_;
  std::string ping_response_;
};

DownloadFeedbackPings::DownloadFeedbackPings(const std::string& ping_request,
                                             const std::string& ping_response)
    : ping_request_(ping_request),
      ping_response_(ping_response) {
}

// static
void DownloadFeedbackPings::CreateForDownload(
    content::DownloadItem* download,
    const std::string& ping_request,
    const std::string& ping_response) {
  DownloadFeedbackPings* pings = new DownloadFeedbackPings(ping_request,
                                                           ping_response);
  download->SetUserData(kPingKey, pings);
}

// static
DownloadFeedbackPings* DownloadFeedbackPings::FromDownload(
    const content::DownloadItem& download) {
  return static_cast<DownloadFeedbackPings*>(download.GetUserData(kPingKey));
}

}  // namespace

DownloadFeedbackService::DownloadFeedbackService(
    net::URLRequestContextGetter* request_context_getter,
    base::TaskRunner* file_task_runner)
    : request_context_getter_(request_context_getter),
      file_task_runner_(file_task_runner),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DownloadFeedbackService::~DownloadFeedbackService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
void DownloadFeedbackService::MaybeStorePingsForDownload(
    DownloadProtectionService::DownloadCheckResult result,
    bool upload_requested,
    content::DownloadItem* download,
    const std::string& ping,
    const std::string& response) {
  // We never upload SAFE files.
  if (result == DownloadProtectionService::SAFE)
    return;

  UMA_HISTOGRAM_BOOLEAN("SBDownloadFeedback.UploadRequestedByServer",
                        upload_requested);
  if (!upload_requested)
    return;

  UMA_HISTOGRAM_COUNTS("SBDownloadFeedback.SizeEligibleKB",
                       download->GetReceivedBytes() / 1024);
  if (download->GetReceivedBytes() > DownloadFeedback::kMaxUploadSize)
    return;

  DownloadFeedbackPings::CreateForDownload(download, ping, response);
}

// static
bool DownloadFeedbackService::IsEnabledForDownload(
    const content::DownloadItem& download) {
  return !!DownloadFeedbackPings::FromDownload(download);
}

// static
bool DownloadFeedbackService::GetPingsForDownloadForTesting(
    const content::DownloadItem& download,
    std::string* ping,
    std::string* response) {
  DownloadFeedbackPings* pings = DownloadFeedbackPings::FromDownload(download);
  if (!pings)
    return false;

  *ping = pings->ping_request();
  *response = pings->ping_response();
  return true;
}

// static
void DownloadFeedbackService::RecordEligibleDownloadShown(
    content::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("SBDownloadFeedback.Eligible",
                            danger_type,
                            content::DOWNLOAD_DANGER_TYPE_MAX);
}

void DownloadFeedbackService::BeginFeedbackForDownload(
    content::DownloadItem* download,
    DownloadCommands::Command download_command) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("SBDownloadFeedback.Activations",
                            download->GetDangerType(),
                            content::DOWNLOAD_DANGER_TYPE_MAX);

  DownloadFeedbackPings* pings = DownloadFeedbackPings::FromDownload(*download);
  DCHECK(pings);

  download->StealDangerousDownload(
      download_command == DownloadCommands::DISCARD,
      base::Bind(&DownloadFeedbackService::BeginFeedbackOrDeleteFile,
                 file_task_runner_, weak_ptr_factory_.GetWeakPtr(),
                 pings->ping_request(), pings->ping_response()));
  if (download_command == DownloadCommands::KEEP)
    DownloadCommands(download).ExecuteCommand(download_command);
}

// static
void DownloadFeedbackService::BeginFeedbackOrDeleteFile(
    const scoped_refptr<base::TaskRunner>& file_task_runner,
    const base::WeakPtr<DownloadFeedbackService>& service,
    const std::string& ping_request,
    const std::string& ping_response,
    const base::FilePath& path) {
  if (service) {
    bool is_path_empty = path.empty();
    UMA_HISTOGRAM_BOOLEAN("SBDownloadFeedback.EmptyFilePathFailure",
                          is_path_empty);
    if (is_path_empty)
      return;
    service->BeginFeedback(ping_request, ping_response, path);
  } else {
    file_task_runner->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&base::DeleteFile), path, false));
  }
}

void DownloadFeedbackService::StartPendingFeedback() {
  DCHECK(!active_feedback_.empty());
  active_feedback_.front()->Start(base::Bind(
      &DownloadFeedbackService::FeedbackComplete, base::Unretained(this)));
}

void DownloadFeedbackService::BeginFeedback(const std::string& ping_request,
                                            const std::string& ping_response,
                                            const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DownloadFeedback> feedback(DownloadFeedback::Create(
      request_context_getter_.get(), file_task_runner_.get(), path,
      ping_request, ping_response));
  active_feedback_.push(std::move(feedback));
  UMA_HISTOGRAM_COUNTS_100("SBDownloadFeedback.ActiveFeedbacks",
                           active_feedback_.size());

  if (active_feedback_.size() == 1)
    StartPendingFeedback();
}

void DownloadFeedbackService::FeedbackComplete() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!active_feedback_.empty());
  active_feedback_.pop();
  if (!active_feedback_.empty())
    StartPendingFeedback();
}

}  // namespace safe_browsing
