// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller.h"

#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/common/url_constants.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(media_router::MediaRouterDialogController);

using content::LoadCommittedDetails;
using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;

namespace media_router {

namespace {

// WebDialogDelegate that specifies what the media router dialog
// will look like.
class MediaRouterDialogDelegate : public WebDialogDelegate {
 public:
  MediaRouterDialogDelegate() {}
  ~MediaRouterDialogDelegate() override {}

  // WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override {
    // Not used, returning dummy value.
    return ui::MODAL_TYPE_WINDOW;
  }

  base::string16 GetDialogTitle() const override {
    return base::string16();
  }

  GURL GetDialogContentURL() const override {
    return GURL(chrome::kChromeUIMediaRouterURL);
  }

  void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const override {
    // MediaRouterUI adds its own message handlers.
  }

  void GetDialogSize(gfx::Size* size) const override;

  std::string GetDialogArgs() const override {
    return std::string();
  }

  void OnDialogClosed(const std::string& json_retval) override {
    // We don't delete |this| here because this class is owned
    // by ConstrainedWebDialogDelegate.
  }

  void OnCloseContents(WebContents* source, bool* out_close_dialog) override {
    if (out_close_dialog)
      *out_close_dialog = true;
  }

  bool ShouldShowDialogTitle() const override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogDelegate);
};

void MediaRouterDialogDelegate::GetDialogSize(gfx::Size* size) const {
  DCHECK(size);
  // TODO(apacible): Remove after autoresizing is implemented for OSX.
#if defined(OS_MACOSX)
  *size = gfx::Size(330, 400);
#else
  // size is not used because the dialog is auto-resizeable.
  *size = gfx::Size();
#endif
}

}  // namespace

class MediaRouterDialogController::DialogWebContentsObserver
    : public content::WebContentsObserver {
 public:
  DialogWebContentsObserver(
      WebContents* web_contents,
      MediaRouterDialogController* dialog_controller)
      : content::WebContentsObserver(web_contents),
        dialog_controller_(dialog_controller) {
  }

 private:
  void WebContentsDestroyed() override {
    // The dialog is already closed. No need to call Close() again.
    // NOTE: |this| is deleted after RemoveObservers() returns.
    dialog_controller_->RemoveObservers();
  }

  void NavigationEntryCommitted(const LoadCommittedDetails& load_details)
      override {
    dialog_controller_->OnDialogNavigated(load_details);
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  MediaRouterDialogController* const dialog_controller_;
};

class MediaRouterDialogController::InitiatorWebContentsObserver
    : public content::WebContentsObserver {
 public:
  InitiatorWebContentsObserver(
      WebContents* web_contents,
      MediaRouterDialogController* dialog_controller)
      : content::WebContentsObserver(web_contents),
        dialog_controller_(dialog_controller) {
  }

 private:
  void WebContentsDestroyed() override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void NavigationEntryCommitted(const LoadCommittedDetails& load_details)
      override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  MediaRouterDialogController* const dialog_controller_;
};

MediaRouterDialogController::MediaRouterDialogController(
    WebContents* web_contents)
    : initiator_(web_contents),
      media_router_dialog_pending_(false) {
  DCHECK(initiator_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

MediaRouterDialogController::~MediaRouterDialogController() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

WebContents* MediaRouterDialogController::ShowMediaRouterDialog() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Get the media router dialog for |initiator|, or create a new dialog
  // if not found.
  WebContents* media_router_dialog = GetMediaRouterDialog();
  if (!media_router_dialog)
    return CreateMediaRouterDialog();

  // Show the initiator holding the existing media router dialog.
  initiator_->GetDelegate()->ActivateContents(initiator_);
  return media_router_dialog;
}

WebContents* MediaRouterDialogController::GetMediaRouterDialog() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return dialog_observer_.get() ? dialog_observer_->web_contents() : nullptr;
}

void MediaRouterDialogController::CloseMediaRouterDialog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initiator_observer_.get());
  WebContents* media_router_dialog = GetMediaRouterDialog();
  CHECK(media_router_dialog);
  RemoveObservers();

  content::WebUI* web_ui = media_router_dialog->GetWebUI();
  if (web_ui) {
    MediaRouterUI* media_router_ui =
        static_cast<MediaRouterUI*>(web_ui->GetController());
    if (media_router_ui)
      media_router_ui->Close();
  }
}

WebContents* MediaRouterDialogController::CreateMediaRouterDialog() {
  DCHECK(!initiator_observer_.get());
  DCHECK(!dialog_observer_.get());

  Profile* profile =
      Profile::FromBrowserContext(initiator_->GetBrowserContext());
  DCHECK(profile);

  WebDialogDelegate* web_dialog_delegate = new MediaRouterDialogDelegate;

  // |web_dialog_delegate|'s owner is |constrained_delegate|.
  // |constrained_delegate| is owned by the parent |views::View|.
  // TODO(apacible): Remove after autoresizing is implemented for OSX.
#if defined(OS_MACOSX)
  ConstrainedWebDialogDelegate* constrained_delegate =
      ShowConstrainedWebDialog(profile, web_dialog_delegate, initiator_);
#else
  // TODO(apacible): Adjust min and max sizes based on new WebUI design.
  gfx::Size min_size = gfx::Size(330, 1);
  gfx::Size max_size = gfx::Size(500, 500);
  // |ShowConstrainedWebDialogWithAutoResize()| will end up creating
  // ConstrainedWebDialogDelegateViewViews containing a WebContents containing
  // the MediaRouterUI, using the provided |web_dialog_delegate|. Then, the
  // view is shown as a modal dialog constrained to the |initiator| WebContents.
  // The dialog will resize between the |min_size| and |max_size| bounds based
  // on the currently rendered contents.
  ConstrainedWebDialogDelegate* constrained_delegate =
      ShowConstrainedWebDialogWithAutoResize(
          profile, web_dialog_delegate, initiator_, min_size, max_size);
#endif

  WebContents* media_router_dialog = constrained_delegate->GetWebContents();

  media_router_dialog_pending_ = true;

  initiator_observer_.reset(new InitiatorWebContentsObserver(
      initiator_, this));
  dialog_observer_.reset(new DialogWebContentsObserver(
      media_router_dialog, this));
  return media_router_dialog;
}

void MediaRouterDialogController::RemoveObservers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initiator_observer_.get());
  DCHECK(dialog_observer_.get());
  initiator_observer_.reset();
  dialog_observer_.reset();
}

void MediaRouterDialogController::OnDialogNavigated(
    const content::LoadCommittedDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WebContents* media_router_dialog = GetMediaRouterDialog();
  CHECK(media_router_dialog);
  ui::PageTransition transition_type = details.entry->GetTransitionType();
  content::NavigationType nav_type = details.type;

  // New |media_router_dialog| is created.
  DCHECK(media_router_dialog_pending_);
  DCHECK(transition_type == ui::PAGE_TRANSITION_AUTO_TOPLEVEL &&
      nav_type == content::NAVIGATION_TYPE_NEW_PAGE)
      << "transition_type: " << transition_type << ", "
      << "nav_type: " << nav_type;

  media_router_dialog_pending_ = false;

  // TODO(imcheng): Initialize dialog.
}

}  // namespace media_router

