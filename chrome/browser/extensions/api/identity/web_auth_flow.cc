// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/web_auth_flow.h"

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/identity_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents.h"
#include "crypto/random.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/guest_view_base.h"
#include "grit/browser_resources.h"
#include "url/gurl.h"

using content::RenderViewHost;
using content::ResourceRedirectDetails;
using content::WebContents;
using content::WebContentsObserver;

namespace extensions {

namespace identity_private = api::identity_private;

WebAuthFlow::WebAuthFlow(
    Delegate* delegate,
    Profile* profile,
    const GURL& provider_url,
    Mode mode)
    : delegate_(delegate),
      profile_(profile),
      provider_url_(provider_url),
      mode_(mode),
      embedded_window_created_(false) {
}

WebAuthFlow::~WebAuthFlow() {
  DCHECK(delegate_ == NULL);

  // Stop listening to notifications first since some of the code
  // below may generate notifications.
  registrar_.RemoveAll();
  WebContentsObserver::Observe(NULL);

  if (!app_window_key_.empty()) {
    AppWindowRegistry::Get(profile_)->RemoveObserver(this);

    if (app_window_ && app_window_->web_contents())
      app_window_->web_contents()->Close();
  }
}

void WebAuthFlow::Start() {
  AppWindowRegistry::Get(profile_)->AddObserver(this);

  // Attach a random ID string to the window so we can recoginize it
  // in OnAppWindowAdded.
  std::string random_bytes;
  crypto::RandBytes(WriteInto(&random_bytes, 33), 32);
  base::Base64Encode(random_bytes, &app_window_key_);

  // identityPrivate.onWebFlowRequest(app_window_key, provider_url_, mode_)
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(app_window_key_);
  args->AppendString(provider_url_.spec());
  if (mode_ == WebAuthFlow::INTERACTIVE)
    args->AppendString("interactive");
  else
    args->AppendString("silent");

  scoped_ptr<Event> event(
      new Event(identity_private::OnWebFlowRequest::kEventName, args.Pass()));
  event->restrict_to_browser_context = profile_;
  ExtensionSystem* system = ExtensionSystem::Get(profile_);

  extensions::ComponentLoader* component_loader =
      system->extension_service()->component_loader();
  if (!component_loader->Exists(extension_misc::kIdentityApiUiAppId)) {
    component_loader->Add(
        IDR_IDENTITY_API_SCOPE_APPROVAL_MANIFEST,
        base::FilePath(FILE_PATH_LITERAL("identity_scope_approval_dialog")));
  }

  system->event_router()->DispatchEventWithLazyListener(
      extension_misc::kIdentityApiUiAppId, event.Pass());
}

void WebAuthFlow::DetachDelegateAndDelete() {
  delegate_ = NULL;
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebAuthFlow::OnAppWindowAdded(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = app_window;
    WebContentsObserver::Observe(app_window->web_contents());

    registrar_.Add(
        this,
        content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
        content::NotificationService::AllBrowserContextsAndSources());
  }
}

void WebAuthFlow::OnAppWindowRemoved(AppWindow* app_window) {
  if (app_window->window_key() == app_window_key_ &&
      app_window->extension_id() == extension_misc::kIdentityApiUiAppId) {
    app_window_ = NULL;
    registrar_.RemoveAll();

    if (delegate_)
      delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
  }
}

void WebAuthFlow::BeforeUrlLoaded(const GURL& url) {
  if (delegate_ && embedded_window_created_)
    delegate_->OnAuthFlowURLChange(url);
}

void WebAuthFlow::AfterUrlLoaded() {
  if (delegate_ && embedded_window_created_ && mode_ == WebAuthFlow::SILENT)
    delegate_->OnAuthFlowFailure(WebAuthFlow::INTERACTION_REQUIRED);
}

void WebAuthFlow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK(app_window_);

  if (!delegate_)
    return;

  if (!embedded_window_created_) {
    DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED);

    RenderViewHost* render_view(
        content::Details<RenderViewHost>(details).ptr());
    WebContents* web_contents = WebContents::FromRenderViewHost(render_view);
    GuestViewBase* guest = GuestViewBase::FromWebContents(web_contents);
    WebContents* embedder = guest ? guest->embedder_web_contents() : NULL;
    if (web_contents &&
        (embedder == WebContentsObserver::web_contents())) {
      // Switch from watching the app window to the guest inside it.
      embedded_window_created_ = true;
      WebContentsObserver::Observe(web_contents);

      registrar_.RemoveAll();
      registrar_.Add(this,
                     content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                     content::Source<WebContents>(web_contents));
      registrar_.Add(this,
                     content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                     content::Source<WebContents>(web_contents));
    }
  } else {
    // embedded_window_created_
    switch (type) {
      case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
        ResourceRedirectDetails* redirect_details =
            content::Details<ResourceRedirectDetails>(details).ptr();
        if (redirect_details != NULL)
          BeforeUrlLoaded(redirect_details->new_url);
        break;
      }
      case content::NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED: {
        std::pair<content::NavigationEntry*, bool>* title =
            content::Details<std::pair<content::NavigationEntry*, bool> >(
                details).ptr();

        if (title->first) {
          delegate_->OnAuthFlowTitleChange(
              base::UTF16ToUTF8(title->first->GetTitle()));
        }
        break;
      }
      default:
        NOTREACHED()
            << "Got a notification that we did not register for: " << type;
        break;
    }
  }
}

void WebAuthFlow::RenderProcessGone(base::TerminationStatus status) {
  if (delegate_)
    delegate_->OnAuthFlowFailure(WebAuthFlow::WINDOW_CLOSED);
}

void WebAuthFlow::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  if (!render_frame_host->GetParent())
    BeforeUrlLoaded(validated_url);
}

void WebAuthFlow::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity",
                               "WebAuthFlow",
                               this,
                               "DidFailProvisionalLoad",
                               "error_code",
                               error_code);
  if (delegate_)
    delegate_->OnAuthFlowFailure(LOAD_FAILED);
}

void WebAuthFlow::DidStopLoading(RenderViewHost* render_view_host) {
  AfterUrlLoaded();
}

void WebAuthFlow::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (delegate_ && details.http_status_code >= 400)
    delegate_->OnAuthFlowFailure(LOAD_FAILED);
}

}  // namespace extensions
