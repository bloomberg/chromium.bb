// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_feedback/md_feedback_dialog_controller.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace {

// The WebDialogDelegate that specifies what the MD Feedback dialog will look
// like.
class MdFeedbackDialogDelegate : public ui::WebDialogDelegate {
 public:
  MdFeedbackDialogDelegate() {}
  ~MdFeedbackDialogDelegate() override {}

  ui::ModalType GetDialogModalType() const override {
    return ui::MODAL_TYPE_SYSTEM;
  }

  base::string16 GetDialogTitle() const override {
    return l10n_util::GetStringUTF16(IDS_MD_FEEDBACK_DIALOG_TITLE);
  }

  GURL GetDialogContentURL() const override {
    return GURL(chrome::kChromeUIFeedbackURL);
  }

  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override {}

  void GetDialogSize(gfx::Size* size) const override {
    // TODO(apacible): Update when WebUI sizing behavior is finalized.
    size->SetSize(400, 600);
  }

  std::string GetDialogArgs() const override {
    return std::string();
  }

  void OnDialogClosed(const std::string& json_retval) override {
    delete this;
  }

  void OnCloseContents(
      content::WebContents* source, bool* out_close_dialog) override {}

  bool ShouldShowDialogTitle() const override {
    return false;
  }

  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    // Do not show the contextual menu.
    return true;
  }
};

}  // namespace

// static
MdFeedbackDialogController* MdFeedbackDialogController::GetInstance() {
  return base::Singleton<MdFeedbackDialogController>::get();
}

void MdFeedbackDialogController::Show(
    content::BrowserContext* browser_context) {
  // TODO(apacible): Platform dependent implementations.
#if !defined(OS_MACOSX)
  // TODO(apacible): If a feedback dialog is already open, bring that dialog
  // to the front rather than creating a new dialog.
  chrome::ShowWebDialog(nullptr, browser_context,
      new MdFeedbackDialogDelegate());
#endif  // !defined(OS_MACOSX)
}

MdFeedbackDialogController::MdFeedbackDialogController() {}
