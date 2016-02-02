// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExperienceSamplingEvent;
using safe_browsing::ClientDownloadResponse;
using safe_browsing::ClientSafeBrowsingReportRequest;
using safe_browsing::download_protection_util::
    GetSBClientDownloadExtensionValueForUMA;

namespace {

const char kDownloadDangerPromptPrefix[] = "Download.DownloadDangerPrompt";

// TODO(wittman): Create a native web contents modal dialog implementation of
// this dialog for non-Views platforms, to support bold formatting of the
// message lead.

// Implements DownloadDangerPrompt using a TabModalConfirmDialog.
class DownloadDangerPromptImpl : public DownloadDangerPrompt,
                                 public content::DownloadItem::Observer,
                                 public TabModalConfirmDialogDelegate {
 public:
  DownloadDangerPromptImpl(content::DownloadItem* item,
                           content::WebContents* web_contents,
                           bool show_context,
                           const OnDone& done);
  ~DownloadDangerPromptImpl() override;

  // DownloadDangerPrompt:
  void InvokeActionForTesting(Action action) override;

 private:
  // content::DownloadItem::Observer:
  void OnDownloadUpdated(content::DownloadItem* download) override;

  // TabModalConfirmDialogDelegate:
  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  base::string16 GetAcceptButtonTitle() override;
  base::string16 GetCancelButtonTitle() override;
  void OnAccepted() override;
  void OnCanceled() override;
  void OnClosed() override;

  void RunDone(Action action);

  content::DownloadItem* download_;
  bool show_context_;
  OnDone done_;

  scoped_ptr<ExperienceSamplingEvent> sampling_event_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptImpl);
};

DownloadDangerPromptImpl::DownloadDangerPromptImpl(
    content::DownloadItem* download,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done)
    : TabModalConfirmDialogDelegate(web_contents),
      download_(download),
      show_context_(show_context),
      done_(done) {
  DCHECK(!done_.is_null());
  download_->AddObserver(this);
  RecordOpenedDangerousConfirmDialog(download_->GetDangerType());

  // ExperienceSampling: A malicious download warning is being shown to the
  // user, so we start a new SamplingEvent and track it.
  sampling_event_.reset(new ExperienceSamplingEvent(
      ExperienceSamplingEvent::kDownloadDangerPrompt, download->GetURL(),
      download->GetReferrerUrl(), download->GetBrowserContext()));
}

DownloadDangerPromptImpl::~DownloadDangerPromptImpl() {
  // |this| might be deleted without invoking any callbacks. E.g. pressing Esc
  // on GTK or if the user navigates away from the page showing the prompt.
  RunDone(DISMISS);
}

void DownloadDangerPromptImpl::InvokeActionForTesting(Action action) {
  switch (action) {
    case ACCEPT:
      Accept();
      break;
    case CANCEL:
      Cancel();
      break;
    case DISMISS:
      RunDone(DISMISS);
      Cancel();
      break;
  }
}

void DownloadDangerPromptImpl::OnDownloadUpdated(
    content::DownloadItem* download) {
  // If the download is nolonger dangerous (accepted externally) or the download
  // is in a terminal state, then the download danger prompt is no longer
  // necessary.
  if (!download->IsDangerous() || download->IsDone()) {
    RunDone(DISMISS);
    Cancel();
  }
}

base::string16 DownloadDangerPromptImpl::GetTitle() {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringUTF16(
          IDS_RESTORE_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    }
    default: {
      return l10n_util::GetStringUTF16(
          IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    }
  }
}

base::string16 DownloadDangerPromptImpl::GetDialogMessage() {
  if (show_context_) {
    switch (download_->GetDangerType()) {
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:  // Fall through
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
        return l10n_util::GetStringFUTF16(
            IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
            download_->GetFileNameToReportUser().LossyDisplayName());
      }
      case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
      case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
      case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
      case content::DOWNLOAD_DANGER_TYPE_MAX: {
        break;
      }
    }
  } else {
    switch (download_->GetDangerType()) {
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
        return l10n_util::GetStringUTF16(
                   IDS_PROMPT_CONFIRM_KEEP_MALICIOUS_DOWNLOAD_LEAD) +
               base::ASCIIToUTF16("\n\n") +
               l10n_util::GetStringUTF16(
                   IDS_PROMPT_CONFIRM_KEEP_MALICIOUS_DOWNLOAD_BODY);
      }
      default: {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_DANGEROUS_DOWNLOAD);
      }
    }
  }
  NOTREACHED();
  return base::string16();
}

base::string16 DownloadDangerPromptImpl::GetAcceptButtonTitle() {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN_MALICIOUS);
    }
    default:
      return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN);
  }
}

base::string16 DownloadDangerPromptImpl::GetCancelButtonTitle() {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringUTF16(IDS_CONFIRM_CANCEL_AGAIN_MALICIOUS);
    }
    default:
      return l10n_util::GetStringUTF16(IDS_CANCEL);
  }
}

void DownloadDangerPromptImpl::OnAccepted() {
  // ExperienceSampling: User proceeded through the warning.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
  RunDone(ACCEPT);
}

void DownloadDangerPromptImpl::OnCanceled() {
  // ExperienceSampling: User canceled the warning.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  RunDone(CANCEL);
}

void DownloadDangerPromptImpl::OnClosed() {
  // ExperienceSampling: User canceled the warning.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  RunDone(DISMISS);
}

void DownloadDangerPromptImpl::RunDone(Action action) {
  // Invoking the callback can cause the download item state to change or cause
  // the constrained window to close, and |callback| refers to a member
  // variable.
  OnDone done = done_;
  done_.Reset();
  if (download_ != NULL) {
    const bool accept = action == DownloadDangerPrompt::ACCEPT;
    RecordDownloadDangerPrompt(accept, *download_);
    if (!download_->GetURL().is_empty() &&
        !download_->GetBrowserContext()->IsOffTheRecord()) {
      SendSafeBrowsingDownloadRecoveryReport(accept, *download_);
    }
    download_->RemoveObserver(this);
    download_ = NULL;
  }
  if (!done.is_null())
    done.Run(action);
}

// Converts DownloadDangerType into their corresponding string.
const char* GetDangerTypeString(
    const content::DownloadDangerType& danger_type) {
  switch (danger_type) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DangerousFile";
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DangerousURL";
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DangerousContent";
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DangerousHost";
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UncommonContent";
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "PotentiallyUnwanted";
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case content::DOWNLOAD_DANGER_TYPE_MAX:
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

#if !defined(USE_AURA)
// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  DownloadDangerPromptImpl* prompt =
      new DownloadDangerPromptImpl(item, web_contents, show_context, done);
  // |prompt| will be deleted when the dialog is done.
  TabModalConfirmDialog::Create(prompt, web_contents);
  return prompt;
}
#endif

void DownloadDangerPrompt::SendSafeBrowsingDownloadRecoveryReport(
    bool did_proceed,
    const content::DownloadItem& download) {
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  ClientSafeBrowsingReportRequest report;
  report.set_type(ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY);
  switch (download.GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      report.set_download_verdict(ClientDownloadResponse::DANGEROUS);
      break;
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      report.set_download_verdict(ClientDownloadResponse::UNCOMMON);
      break;
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      report.set_download_verdict(ClientDownloadResponse::POTENTIALLY_UNWANTED);
      break;
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      report.set_download_verdict(ClientDownloadResponse::DANGEROUS_HOST);
      break;
    default:
      break;
  }
  report.set_url(download.GetURL().spec());
  report.set_did_proceed(did_proceed);

  std::string serialized_report;
  if (report.SerializeToString(&serialized_report))
    sb_service->SendDownloadRecoveryReport(serialized_report);
  else
    DLOG(ERROR) << "Unable to serialize the threat report.";
}

void DownloadDangerPrompt::RecordDownloadDangerPrompt(
    bool did_proceed,
    const content::DownloadItem& download) {
  int dangerous_file_type =
      GetSBClientDownloadExtensionValueForUMA(download.GetTargetFilePath());
  content::DownloadDangerType danger_type = download.GetDangerType();

  UMA_HISTOGRAM_SPARSE_SLOWLY(
      base::StringPrintf("%s.%s.Shown", kDownloadDangerPromptPrefix,
                         GetDangerTypeString(danger_type)),
      dangerous_file_type);
  if (did_proceed) {
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        base::StringPrintf("%s.%s.Proceed", kDownloadDangerPromptPrefix,
                           GetDangerTypeString(danger_type)),
        dangerous_file_type);
  }
}
