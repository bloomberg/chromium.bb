// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "base/bind.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Implements DownloadDangerPrompt using a TabModalConfirmDialog.
class DownloadDangerPromptImpl
  : public DownloadDangerPrompt,
    public content::DownloadItem::Observer,
    public TabModalConfirmDialogDelegate {
 public:
  DownloadDangerPromptImpl(content::DownloadItem* item,
                           content::WebContents* web_contents,
                           bool show_context,
                           const base::Closure& accepted,
                           const base::Closure& canceled);
  virtual ~DownloadDangerPromptImpl();

  // DownloadDangerPrompt
  virtual void InvokeActionForTesting(Action action) OVERRIDE;

 private:
  // content::DownloadItem::Observer
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

  // TabModalConfirmDialogDelegate
  virtual string16 GetTitle() OVERRIDE;
  virtual string16 GetMessage() OVERRIDE;
  virtual string16 GetAcceptButtonTitle() OVERRIDE;
  virtual void OnAccepted() OVERRIDE;
  virtual void OnCanceled() OVERRIDE;

  // Runs |callback|. PrepareToClose() is called beforehand. Doing so prevents
  // this object from responding to state changes in |download_| that might
  // result from invoking the callback. |callback| must refer to either
  // |accepted_| or |canceled_|.
  void RunCallback(const base::Closure& callback);

  // Resets |accepted_|, |canceled_| and removes the observer from |download_|,
  // in preparation for closing the prompt.
  void PrepareToClose();

  content::DownloadItem* download_;
  bool show_context_;
  base::Closure accepted_;
  base::Closure canceled_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDangerPromptImpl);
};

DownloadDangerPromptImpl::DownloadDangerPromptImpl(
    content::DownloadItem* download,
    content::WebContents* web_contents,
    bool show_context,
    const base::Closure& accepted,
    const base::Closure& canceled)
    : TabModalConfirmDialogDelegate(web_contents),
      download_(download),
      show_context_(show_context),
      accepted_(accepted),
      canceled_(canceled) {
  DCHECK(!accepted_.is_null());
  // canceled_ is allowed to be null.
  DCHECK(download_);
  download_->AddObserver(this);
}

DownloadDangerPromptImpl::~DownloadDangerPromptImpl() {
  // |this| might be deleted without invoking any callbacks. E.g. pressing Esc
  // on GTK or if the user navigates away from the page showing the prompt.
  PrepareToClose();
}

void DownloadDangerPromptImpl::InvokeActionForTesting(Action action) {
  if (action == ACCEPT)
    Accept();
  else
    Cancel();
}

void DownloadDangerPromptImpl::OnDownloadUpdated(
    content::DownloadItem* download) {
  // If the download is nolonger dangerous (accepted externally) or the download
  // doesn't exist anymore, the download danger prompt is no longer necessary.
  if (!download->IsInProgress() || !download->IsDangerous())
    Cancel();
}

void DownloadDangerPromptImpl::OnDownloadOpened(
    content::DownloadItem* download) {
}

string16 DownloadDangerPromptImpl::GetTitle() {
  return l10n_util::GetStringUTF16(IDS_CONFIRM_KEEP_DANGEROUS_DOWNLOAD_TITLE);
}

string16 DownloadDangerPromptImpl::GetMessage() {
  if (!show_context_)
    return l10n_util::GetStringUTF16(
        IDS_PROMPT_CONFIRM_KEEP_DANGEROUS_DOWNLOAD);
  switch (download_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_DANGEROUS_DOWNLOAD,
          download_->GetFileNameToReportUser().LossyDisplayName());
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL: // Fall through
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
          download_->GetFileNameToReportUser().LossyDisplayName());
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return l10n_util::GetStringFUTF16(
          IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
          download_->GetFileNameToReportUser().LossyDisplayName());
    default:
      NOTREACHED();
      return string16();
  }
}

string16 DownloadDangerPromptImpl::GetAcceptButtonTitle() {
  return l10n_util::GetStringUTF16(
      show_context_ ? IDS_CONFIRM_DOWNLOAD : IDS_CONFIRM_DOWNLOAD_AGAIN);
}

void DownloadDangerPromptImpl::OnAccepted() {
  RunCallback(accepted_);
}

void DownloadDangerPromptImpl::OnCanceled() {
  RunCallback(canceled_);
}

void DownloadDangerPromptImpl::RunCallback(const base::Closure& callback) {
  // Invoking the callback can cause the download item state to change or cause
  // the constrained window to close, and |callback| refers to a member
  // variable.
  base::Closure callback_copy = callback;
  PrepareToClose();
  if (!callback_copy.is_null())
    callback_copy.Run();
}

void DownloadDangerPromptImpl::PrepareToClose() {
  accepted_.Reset();
  canceled_.Reset();
  if (download_ != NULL) {
    download_->RemoveObserver(this);
    download_ = NULL;
  }
}

}  // namespace

// static
DownloadDangerPrompt* DownloadDangerPrompt::Create(
    content::DownloadItem* item,
    content::WebContents* web_contents,
    bool show_context,
    const base::Closure& accepted,
    const base::Closure& canceled) {
  DownloadDangerPromptImpl* prompt = new DownloadDangerPromptImpl(
      item, web_contents, show_context, accepted, canceled);
  // |prompt| will be deleted when the dialog is done.
  TabModalConfirmDialog::Create(prompt, web_contents);
  return prompt;
}
