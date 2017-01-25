// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "url/gurl.h"

using extensions::ExperienceSamplingEvent;
using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

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
  // If show_context_ is true, this is a download confirmation dialog by
  // download API, otherwise it is download recovery dialog from a regular
  // download.
  bool show_context_;
  OnDone done_;

  std::unique_ptr<ExperienceSamplingEvent> sampling_event_;

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
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return l10n_util::GetStringUTF16(IDS_KEEP_DANGEROUS_DOWNLOAD_TITLE);
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return l10n_util::GetStringUTF16(IDS_KEEP_UNCOMMON_DOWNLOAD_TITLE);
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
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
        return l10n_util::GetStringUTF16(
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
  return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN);
}

base::string16 DownloadDangerPromptImpl::GetCancelButtonTitle() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
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
    // If this download is no longer dangerous, or is already canceled or
    // completed, don't send any report.
    if (download_->IsDangerous() && !download_->IsDone()) {
      const bool accept = action == DownloadDangerPrompt::ACCEPT;
      RecordDownloadDangerPrompt(accept, *download_);
      if (!download_->GetURL().is_empty() &&
          !download_->GetBrowserContext()->IsOffTheRecord()) {
        ClientSafeBrowsingReportRequest::ReportType report_type
            = show_context_ ?
                ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_BY_API :
                ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY;
        SendSafeBrowsingDownloadReport(report_type, accept, *download_);
      }
    }
    download_->RemoveObserver(this);
    download_ = NULL;
  }
  if (!done.is_null())
    done.Run(action);
}

}  // namespace

// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    return DownloadDangerPrompt::CreateDownloadDangerPromptViews(
        item, web_contents, show_context, done);
  }

  DownloadDangerPromptImpl* prompt =
      new DownloadDangerPromptImpl(item, web_contents, show_context, done);
  // |prompt| will be deleted when the dialog is done.
  TabModalConfirmDialog::Create(prompt, web_contents);
  return prompt;
}
