// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/compiler_specific.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_features.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

using extensions::ExperienceSamplingEvent;
using safe_browsing::ClientSafeBrowsingReportRequest;

namespace {

const int kMessageWidth = 320;

// Views-specific implementation of download danger prompt dialog. We use this
// class rather than a TabModalConfirmDialog so that we can use custom
// formatting on the text in the body of the dialog.
class DownloadDangerPromptViews : public DownloadDangerPrompt,
                                  public content::DownloadItem::Observer,
                                  public views::DialogDelegate {
 public:
  DownloadDangerPromptViews(content::DownloadItem* item,
                            bool show_context,
                            const OnDone& done);

  // DownloadDangerPrompt methods:
  void InvokeActionForTesting(Action action) override;

  // views::DialogDelegate methods:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  bool Cancel() override;
  bool Accept() override;
  bool Close() override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  // content::DownloadItem::Observer:
  void OnDownloadUpdated(content::DownloadItem* download) override;

 private:
  base::string16 GetAcceptButtonTitle() const;
  base::string16 GetCancelButtonTitle() const;
  base::string16 GetMessageBody() const;
  void RunDone(Action action);

  content::DownloadItem* download_;
  // If show_context_ is true, this is a download confirmation dialog by
  // download API, otherwise it is download recovery dialog from a regular
  // download.
  bool show_context_;
  OnDone done_;

  std::unique_ptr<ExperienceSamplingEvent> sampling_event_;

  views::View* contents_view_;
};

DownloadDangerPromptViews::DownloadDangerPromptViews(
    content::DownloadItem* item,
    bool show_context,
    const OnDone& done)
    : download_(item),
      show_context_(show_context),
      done_(done),
      contents_view_(NULL) {
  DCHECK(!done_.is_null());
  download_->AddObserver(this);

  contents_view_ = new views::View;

  views::GridLayout* layout = views::GridLayout::CreatePanel(contents_view_);
  contents_view_->SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, kMessageWidth, 0);

  views::Label* message_body_label = new views::Label(GetMessageBody());
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetAllowCharacterBreak(true);

  layout->StartRow(0, 0);
  layout->AddView(message_body_label);

  RecordOpenedDangerousConfirmDialog(download_->GetDangerType());

  // ExperienceSampling: A malicious download warning is being shown to the
  // user, so we start a new SamplingEvent and track it.
  sampling_event_.reset(new ExperienceSamplingEvent(
      ExperienceSamplingEvent::kDownloadDangerPrompt,
      item->GetURL(),
      item->GetReferrerUrl(),
      item->GetBrowserContext()));
}

// DownloadDangerPrompt methods:
void DownloadDangerPromptViews::InvokeActionForTesting(Action action) {
  switch (action) {
    case ACCEPT:
      // This inversion is intentional.
      Cancel();
      break;

    case DISMISS:
      Close();
      break;

    case CANCEL:
      Accept();
      break;

    default:
      NOTREACHED();
      break;
  }
}

// views::DialogDelegate methods:
base::string16 DownloadDangerPromptViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return GetAcceptButtonTitle();

    case ui::DIALOG_BUTTON_CANCEL:
      return GetCancelButtonTitle();

    default:
      return DialogDelegate::GetDialogButtonLabel(button);
  }
}

base::string16 DownloadDangerPromptViews::GetWindowTitle() const {
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

void DownloadDangerPromptViews::DeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

ui::ModalType DownloadDangerPromptViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool DownloadDangerPromptViews::Accept() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // ExperienceSampling: User did not proceed down the dangerous path.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  // Note that the presentational concept of "Accept/Cancel" is inverted from
  // the model's concept of ACCEPT/CANCEL. In the UI, the safe path is "Accept"
  // and the dangerous path is "Cancel".
  RunDone(CANCEL);
  return true;
}

bool DownloadDangerPromptViews::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // ExperienceSampling: User proceeded down the dangerous path.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kProceed);
  RunDone(ACCEPT);
  return true;
}

bool DownloadDangerPromptViews::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // ExperienceSampling: User did not proceed down the dangerous path.
  sampling_event_->CreateUserDecisionEvent(ExperienceSamplingEvent::kDeny);
  RunDone(DISMISS);
  return true;
}

views::View* DownloadDangerPromptViews::GetContentsView() {
  return contents_view_;
}

views::Widget* DownloadDangerPromptViews::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* DownloadDangerPromptViews::GetWidget() const {
  return contents_view_->GetWidget();
}

// content::DownloadItem::Observer:
void DownloadDangerPromptViews::OnDownloadUpdated(
    content::DownloadItem* download) {
  // If the download is nolonger dangerous (accepted externally) or the download
  // is in a terminal state, then the download danger prompt is no longer
  // necessary.
  if (!download_->IsDangerous() || download_->IsDone()) {
    RunDone(DISMISS);
    Cancel();
  }
}

base::string16 DownloadDangerPromptViews::GetAcceptButtonTitle() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 DownloadDangerPromptViews::GetCancelButtonTitle() const {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD);
  return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN);
}

base::string16 DownloadDangerPromptViews::GetMessageBody() const {
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

void DownloadDangerPromptViews::RunDone(Action action) {
  // Invoking the callback can cause the download item state to change or cause
  // the window to close, and |callback| refers to a member variable.
  OnDone done = done_;
  done_.Reset();
  if (download_ != NULL) {
    // If this download is no longer dangerous, is already canceled or
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

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  return DownloadDangerPrompt::CreateDownloadDangerPromptViews(
      item, web_contents, show_context, done);
}
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER

// static
DownloadDangerPrompt* DownloadDangerPrompt::CreateDownloadDangerPromptViews(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  DownloadDangerPromptViews* download_danger_prompt =
      new DownloadDangerPromptViews(item, show_context, done);
  constrained_window::ShowWebModalDialogViews(download_danger_prompt,
                                              web_contents);
  return download_danger_prompt;
}
