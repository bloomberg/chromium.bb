// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

const int kMessageWidth = 320;
const int kParagraphPadding = 15;

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
  virtual void InvokeActionForTesting(Action action) OVERRIDE;

  // views::DialogDelegate methods:
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Close() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // content::DownloadItem::Observer:
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;

 private:
  base::string16 GetAcceptButtonTitle() const;
  base::string16 GetCancelButtonTitle() const;
  // The message lead is separated from the main text and is bolded.
  base::string16 GetMessageLead() const;
  base::string16 GetMessageBody() const;
  void RunDone(Action action);

  content::DownloadItem* download_;
  bool show_context_;
  OnDone done_;

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

  const base::string16 message_lead = GetMessageLead();

  if (!message_lead.empty()) {
    ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
    views::Label* message_lead_label = new views::Label(
        message_lead, rb->GetFontList(ui::ResourceBundle::BoldFont));
    message_lead_label->SetMultiLine(true);
    message_lead_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_lead_label->SetAllowCharacterBreak(true);

    layout->StartRow(0, 0);
    layout->AddView(message_lead_label);

    layout->AddPaddingRow(0, kParagraphPadding);
  }

  views::Label* message_body_label = new views::Label(GetMessageBody());
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetAllowCharacterBreak(true);

  layout->StartRow(0, 0);
  layout->AddView(message_body_label);

  RecordOpenedDangerousConfirmDialog(download_->GetDangerType());
}

// DownloadDangerPrompt methods:
void DownloadDangerPromptViews::InvokeActionForTesting(Action action) {
  switch (action) {
    case ACCEPT:
      Accept();
      break;

    case CANCEL:
    case DISMISS:
      Cancel();
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
  else
    return l10n_util::GetStringUTF16(IDS_RESTORE_KEEP_DANGEROUS_DOWNLOAD_TITLE);
}

void DownloadDangerPromptViews::DeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete this;
}

ui::ModalType DownloadDangerPromptViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool DownloadDangerPromptViews::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RunDone(CANCEL);
  return true;
}

bool DownloadDangerPromptViews::Accept() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RunDone(ACCEPT);
  return true;
}

bool DownloadDangerPromptViews::Close() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RunDone(DISMISS);
  return true;
}

views::View* DownloadDangerPromptViews::GetInitiallyFocusedView() {
  return GetDialogClientView()->cancel_button();
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
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
      return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN_MALICIOUS);
    }
    default:
      return l10n_util::GetStringUTF16(IDS_CONFIRM_DOWNLOAD_AGAIN);
  }
}

base::string16 DownloadDangerPromptViews::GetCancelButtonTitle() const {
  if (show_context_)
    return l10n_util::GetStringUTF16(IDS_CANCEL);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
      return l10n_util::GetStringUTF16(IDS_CONFIRM_CANCEL_AGAIN_MALICIOUS);
    }
    default:
      return l10n_util::GetStringUTF16(IDS_CANCEL);
  }
}

base::string16 DownloadDangerPromptViews::GetMessageLead() const {
  if (!show_context_) {
    switch (download_->GetDangerType()) {
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_CONFIRM_KEEP_MALICIOUS_DOWNLOAD_LEAD);

      default:
        break;
    }
  }

  return base::string16();
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
      case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
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
    download_->RemoveObserver(this);
    download_ = NULL;
  }
  if (!done.is_null())
    done.Run(action);
}

}  // namespace

DownloadDangerPrompt* DownloadDangerPrompt::Create(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const OnDone& done) {
  DownloadDangerPromptViews* download_danger_prompt =
      new DownloadDangerPromptViews(item, show_context, done);
  ShowWebModalDialogViews(download_danger_prompt, web_contents);
  return download_danger_prompt;
}
