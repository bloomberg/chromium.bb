// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_ui_service.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/common/url_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::MediaRouterDialogControllerImpl);

using content::LoadCommittedDetails;
using content::NavigationController;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;

namespace media_router {

namespace {

constexpr const int kMaxHeight = 2000;
constexpr const int kMinHeight = 80;
constexpr const int kWidth = 340;

// WebDialogDelegate that specifies what the Media Router dialog
// will look like.
class MediaRouterDialogDelegate : public WebDialogDelegate {
 public:
  explicit MediaRouterDialogDelegate(
      const base::WeakPtr<MediaRouterDialogControllerImpl>& controller)
      : controller_(controller) {}
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

  void GetDialogSize(gfx::Size* size) const override {
    DCHECK(size);
    // We set the dialog width if it's not set, so that the dialog is
    // center-aligned horizontally when it appears.
    if (size->width() != kWidth)
      size->set_width(kWidth);
    // GetDialogSize() is called when the browser window resizes. We may want to
    // update the maximum height of the dialog and scale the WebUI to the new
    // height. |size| is not set because the dialog is auto-resizeable.
    controller_->UpdateMaxDialogSize();
  }

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
  base::WeakPtr<MediaRouterAction> action_;
  base::WeakPtr<MediaRouterDialogControllerImpl> controller_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogDelegate);
};

MediaRouterActionController* GetActionController(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return MediaRouterUIService::Get(profile)->action_controller();
}

}  // namespace

// static
MediaRouterDialogControllerImpl*
MediaRouterDialogControllerImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerImpl::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerImpl::FromWebContents(web_contents);
}

class MediaRouterDialogControllerImpl::DialogWebContentsObserver
    : public content::WebContentsObserver {
 public:
  DialogWebContentsObserver(
      WebContents* web_contents,
      MediaRouterDialogControllerImpl* dialog_controller)
      : content::WebContentsObserver(web_contents),
        dialog_controller_(dialog_controller) {
  }

 private:
  void WebContentsDestroyed() override {
    // The dialog is already closed. No need to call Close() again.
    // NOTE: |this| is deleted after Reset() returns.
    dialog_controller_->Reset();
  }

  void NavigationEntryCommitted(const LoadCommittedDetails& load_details)
      override {
    dialog_controller_->OnDialogNavigated(load_details);
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  MediaRouterDialogControllerImpl* const dialog_controller_;
};

MediaRouterDialogControllerImpl::MediaRouterDialogControllerImpl(
    WebContents* web_contents)
    : MediaRouterDialogController(web_contents),
      media_router_dialog_pending_(false),
      action_controller_(GetActionController(web_contents)),
      weak_ptr_factory_(this) {
  DCHECK(action_controller_);
}

MediaRouterDialogControllerImpl::~MediaRouterDialogControllerImpl() {
  Reset();
}

WebContents* MediaRouterDialogControllerImpl::GetMediaRouterDialog() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return dialog_observer_.get() ? dialog_observer_->web_contents() : nullptr;
}

void MediaRouterDialogControllerImpl::SetMediaRouterAction(
    const base::WeakPtr<MediaRouterAction>& action) {
  action_ = action;
}

bool MediaRouterDialogControllerImpl::IsShowingMediaRouterDialog() const {
  return GetMediaRouterDialog() != nullptr;
}

void MediaRouterDialogControllerImpl::UpdateMaxDialogSize() {
  WebContents* media_router_dialog = GetMediaRouterDialog();
  if (!media_router_dialog)
    return;

  content::WebUI* web_ui = media_router_dialog->GetWebUI();
  if (web_ui) {
    MediaRouterUI* media_router_ui =
        static_cast<MediaRouterUI*>(web_ui->GetController());
    if (media_router_ui) {
      Browser* browser = chrome::FindBrowserWithWebContents(initiator());
      web_modal::WebContentsModalDialogHost* host = nullptr;
      if (browser)
        host = browser->window()->GetWebContentsModalDialogHost();

      gfx::Size maxSize = host ?
          host->GetMaximumDialogSize() :
          initiator()->GetContainerBounds().size();

      // The max height of the dialog should be 90% of the browser window
      // height. The width stays fixed.
      maxSize.Enlarge(0, -0.1 * maxSize.height());
      media_router_ui->UpdateMaxDialogHeight(maxSize.height());
    }
  }
}

void MediaRouterDialogControllerImpl::CloseMediaRouterDialog() {
  WebContents* media_router_dialog = GetMediaRouterDialog();
  if (!media_router_dialog)
    return;

  content::WebUI* web_ui = media_router_dialog->GetWebUI();
  if (web_ui) {
    MediaRouterUI* media_router_ui =
        static_cast<MediaRouterUI*>(web_ui->GetController());
    if (media_router_ui)
      media_router_ui->Close();
  }
}

void MediaRouterDialogControllerImpl::CreateMediaRouterDialog() {
  DCHECK(!dialog_observer_.get());

  base::Time dialog_creation_time = base::Time::Now();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media_router", "UI", initiator());

  Profile* profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  DCHECK(profile);

  // |web_dialog_delegate|'s owner is |constrained_delegate|.
  // |constrained_delegate| is owned by the parent |views::View|.
  WebDialogDelegate* web_dialog_delegate =
      new MediaRouterDialogDelegate(weak_ptr_factory_.GetWeakPtr());

  // |ShowConstrainedWebDialogWithAutoResize()| will end up creating
  // ConstrainedWebDialogDelegateViewViews containing a WebContents containing
  // the MediaRouterUI, using the provided |web_dialog_delegate|. Then, the
  // view is shown as a modal dialog constrained to the |initiator| WebContents.
  // The dialog will resize between the given minimum and maximum size bounds
  // based on the currently rendered contents.
  ConstrainedWebDialogDelegate* constrained_delegate =
      ShowConstrainedWebDialogWithAutoResize(
          profile, web_dialog_delegate, initiator(),
              gfx::Size(kWidth, kMinHeight), gfx::Size(kWidth, kMaxHeight));

  WebContents* media_router_dialog = constrained_delegate->GetWebContents();
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("media_router", "UI", initiator(),
                                      "WebContents created",
                                      media_router_dialog);

  // |media_router_ui| is created when |constrained_delegate| is created.
  // For tests, GetWebUI() returns a nullptr.
  if (media_router_dialog->GetWebUI()) {
    MediaRouterUI* media_router_ui = static_cast<MediaRouterUI*>(
        media_router_dialog->GetWebUI()->GetController());
    DCHECK(media_router_ui);
    media_router_ui->SetUIInitializationTimer(dialog_creation_time);
  }

  media_router_dialog_pending_ = true;

  dialog_observer_.reset(new DialogWebContentsObserver(
      media_router_dialog, this));

  // The |action_controller_| must be notified after |action_| to avoid a UI
  // bug in which the drop shadow is drawn in an incorrect position.
  if (action_)
    action_->OnDialogShown();
  action_controller_->OnDialogShown();
}

void MediaRouterDialogControllerImpl::Reset() {
  if (IsShowingMediaRouterDialog()) {
    if (action_)
      action_->OnDialogHidden();
    action_controller_->OnDialogHidden();
  }
  MediaRouterDialogController::Reset();
  dialog_observer_.reset();
}

void MediaRouterDialogControllerImpl::OnDialogNavigated(
    const content::LoadCommittedDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  WebContents* media_router_dialog = GetMediaRouterDialog();
  CHECK(media_router_dialog);
  ui::PageTransition transition_type = details.entry->GetTransitionType();
  content::NavigationType nav_type = details.type;

  // New |media_router_dialog| is created.
  DCHECK(media_router_dialog_pending_);
  DCHECK(ui::PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL) &&
      nav_type == content::NAVIGATION_TYPE_NEW_PAGE)
      << "transition_type: " << transition_type << ", "
      << "nav_type: " << nav_type;

  media_router_dialog_pending_ = false;

  PopulateDialog(media_router_dialog);
}

void MediaRouterDialogControllerImpl::PopulateDialog(
    content::WebContents* media_router_dialog) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(media_router_dialog);
  if (!initiator() || !media_router_dialog->GetWebUI()) {
    Reset();
    return;
  }

  MediaRouterUI* media_router_ui = static_cast<MediaRouterUI*>(
      media_router_dialog->GetWebUI()->GetController());
  DCHECK(media_router_ui);

  std::unique_ptr<CreatePresentationConnectionRequest>
      create_connection_request(TakeCreateConnectionRequest());
  PresentationServiceDelegateImpl* delegate =
      PresentationServiceDelegateImpl::FromWebContents(initiator());
  if (!create_connection_request.get()) {
    media_router_ui->InitWithDefaultMediaSource(initiator(), delegate);
  } else {
    media_router_ui->InitWithPresentationSessionRequest(
        initiator(), delegate, std::move(create_connection_request));
  }
}

}  // namespace media_router
