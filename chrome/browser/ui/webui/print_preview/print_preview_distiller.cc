// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_distiller.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/prerender_messages.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

using content::OpenURLParams;
using content::RenderViewHost;
using content::SessionStorageNamespace;
using content::WebContents;

class PrintPreviewDistiller::WebContentsDelegateImpl
    : public content::WebContentsDelegate,
      public content::NotificationObserver,
      public content::WebContentsObserver {
 public:
  explicit WebContentsDelegateImpl(WebContents* web_contents,
                                   scoped_ptr<base::DictionaryValue> settings,
                                   const base::Closure on_failed_callback)
      : content::WebContentsObserver(web_contents),
        settings_(settings.Pass()),
        on_failed_callback_(on_failed_callback) {
    web_contents->SetDelegate(this);

    // Close ourselves when the application is shutting down.
    notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                                content::NotificationService::AllSources());

    // Register to inform new RenderViews that we're rendering.
    notification_registrar_.Add(
        this, content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
        content::Source<WebContents>(web_contents));
  }

  ~WebContentsDelegateImpl() override { web_contents()->SetDelegate(nullptr); }

  // content::WebContentsDelegate implementation.
  WebContents* OpenURLFromTab(WebContents* source,
                              const OpenURLParams& params) override {
    on_failed_callback_.Run();
    return nullptr;
  }

  void CloseContents(content::WebContents* contents) override {
    on_failed_callback_.Run();
  }

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override {
    on_failed_callback_.Run();
    // Cancel the download.
    callback.Run(false);
  }

  bool ShouldCreateWebContents(
      WebContents* web_contents,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      SessionStorageNamespace* session_storage_namespace) override {
    // Since we don't want to permit child windows that would have a
    // window.opener property, terminate rendering.
    on_failed_callback_.Run();
    // Cancel the popup.
    return false;
  }

  bool OnGoToEntryOffset(int offset) override {
    // This isn't allowed because the history merge operation
    // does not work if there are renderer issued challenges.
    // TODO(cbentzel): Cancel in this case? May not need to do
    // since render-issued offset navigations are not guaranteed,
    // but indicates that the page cares about the history.
    return false;
  }

  bool ShouldSuppressDialogs(WebContents* source) override {
    // We still want to show the user the message when they navigate to this
    // page, so cancel this render.
    on_failed_callback_.Run();
    // Always suppress JavaScript messages if they're triggered by a page being
    // rendered.
    return true;
  }

  void RegisterProtocolHandler(WebContents* web_contents,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override {
    on_failed_callback_.Run();
  }

  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    // When a new RenderFrame is created for a distilled rendering
    // WebContents, tell the new RenderFrame it's being used for
    // prerendering before any navigations occur.  Note that this is
    // always triggered before the first navigation, so there's no
    // need to send the message just after the WebContents is created.
    render_frame_host->Send(new PrerenderMsg_SetIsPrerendering(
        render_frame_host->GetRoutingID(), true));
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // Ask the page to trigger an anchor navigation once the distilled
    // contents are added to the page.
    dom_distiller::RunIsolatedJavaScript(
        web_contents()->GetMainFrame(),
        "navigate_on_initial_content_load = true;");
  }

  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    // The second content loads signals that the distilled contents have
    // been delivered to the page via inline JavaScript execution.
    if (web_contents()->GetController().GetEntryCount() > 1) {
      RenderViewHost* rvh = web_contents()->GetRenderViewHost();
      rvh->Send(new PrintMsg_InitiatePrintPreview(rvh->GetRoutingID(), false));
      rvh->Send(new PrintMsg_PrintPreview(rvh->GetRoutingID(), *settings_));
    }
  }

  void DidGetRedirectForResourceRequest(
      content::RenderFrameHost* render_frame_host,
      const content::ResourceRedirectDetails& details) override {
    if (details.resource_type != content::RESOURCE_TYPE_MAIN_FRAME)
      return;
    // Redirects are unsupported for distilled content renderers.
    on_failed_callback_.Run();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    on_failed_callback_.Run();
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      // TODO(davidben): Try to remove this in favor of relying on
      // FINAL_STATUS_PROFILE_DESTROYED.
      case chrome::NOTIFICATION_APP_TERMINATING:
        on_failed_callback_.Run();
        return;

      case content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED: {
        if (web_contents()) {
          DCHECK_EQ(content::Source<WebContents>(source).ptr(), web_contents());

          // Make sure the size of the RenderViewHost has been passed to the new
          // RenderView.  Otherwise, the size may not be sent until the
          // RenderViewReady event makes it from the render process to the UI
          // thread of the browser process.  When the RenderView receives its
          // size, is also sets itself to be visible, which would then break the
          // visibility API.
          content::Details<RenderViewHost> new_render_view_host(details);
          new_render_view_host->GetWidget()->WasResized();
          web_contents()->WasHidden();
        }
        break;
      }

      default:
        NOTREACHED() << "Unexpected notification sent.";
        break;
    }
  }

 private:
  scoped_ptr<base::DictionaryValue> settings_;
  content::NotificationRegistrar notification_registrar_;

  // The callback called when the preview failed.
  base::Closure on_failed_callback_;
};

const base::Feature PrintPreviewDistiller::kFeature = {
    "PrintPreviewDistiller", base::FEATURE_ENABLED_BY_DEFAULT,
};

bool PrintPreviewDistiller::IsEnabled() {
  return base::FeatureList::IsEnabled(kFeature);
}

PrintPreviewDistiller::PrintPreviewDistiller(
    WebContents* source_web_contents,
    const base::Closure on_failed_callback,
    scoped_ptr<base::DictionaryValue> settings) {
  content::SessionStorageNamespace* session_storage_namespace =
      source_web_contents->GetController().GetDefaultSessionStorageNamespace();
  CreateDestinationWebContents(session_storage_namespace, source_web_contents,
                               settings.Pass(), on_failed_callback);

  DCHECK(web_contents_);
  ::DistillAndView(source_web_contents, web_contents_.get());
}

void PrintPreviewDistiller::CreateDestinationWebContents(
    SessionStorageNamespace* session_storage_namespace,
    WebContents* source_web_contents,
    scoped_ptr<base::DictionaryValue> settings,
    const base::Closure on_failed_callback) {
  DCHECK(!web_contents_);

  web_contents_.reset(
      CreateWebContents(session_storage_namespace, source_web_contents));

  printing::PrintPreviewMessageHandler::CreateForWebContents(
      web_contents_.get());

  web_contents_delegate_.reset(new WebContentsDelegateImpl(
      web_contents_.get(), settings.Pass(), on_failed_callback));

  // Set the size of the distilled WebContents.
  ResizeWebContents(web_contents_.get(), gfx::Size(1, 1));

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return;

  dialog_controller->AddProxyDialogForWebContents(web_contents_.get(),
                                                  source_web_contents);
}

PrintPreviewDistiller::~PrintPreviewDistiller() {
  if (web_contents_) {
    printing::PrintPreviewDialogController* dialog_controller =
        printing::PrintPreviewDialogController::GetInstance();
    if (!dialog_controller)
      return;

    dialog_controller->RemoveProxyDialogForWebContents(web_contents_.get());
  }
}

WebContents* PrintPreviewDistiller::CreateWebContents(
    SessionStorageNamespace* session_storage_namespace,
    WebContents* source_web_contents) {
  // TODO(ajwong): Remove the temporary map once prerendering is aware of
  // multiple session storage namespaces per tab.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  Profile* profile =
      Profile::FromBrowserContext(source_web_contents->GetBrowserContext());
  session_storage_namespace_map[std::string()] = session_storage_namespace;
  return WebContents::CreateWithSessionStorage(
      WebContents::CreateParams(profile), session_storage_namespace_map);
}
