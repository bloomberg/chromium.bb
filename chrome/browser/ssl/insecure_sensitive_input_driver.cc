// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_sensitive_input_driver.h"

#include <utility>

#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "chrome/browser/ssl/visible_password_observer.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

InsecureSensitiveInputDriver::InsecureSensitiveInputDriver(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  // Does nothing if a VisiblePasswordObserver has already been created
  // for this WebContents.
  VisiblePasswordObserver::CreateForWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
}

InsecureSensitiveInputDriver::~InsecureSensitiveInputDriver() {}

void InsecureSensitiveInputDriver::BindInsecureInputServiceRequest(
    blink::mojom::InsecureInputServiceRequest request) {
  insecure_input_bindings_.AddBinding(this, std::move(request));
}

void InsecureSensitiveInputDriver::PasswordFieldVisibleInInsecureContext() {
  VisiblePasswordObserver* observer = VisiblePasswordObserver::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
  observer->RenderFrameHasVisiblePasswordField(render_frame_host_);
}

void InsecureSensitiveInputDriver::
    AllPasswordFieldsInInsecureContextInvisible() {
  VisiblePasswordObserver* observer = VisiblePasswordObserver::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host_));
  observer->RenderFrameHasNoVisiblePasswordFields(render_frame_host_);
}

void InsecureSensitiveInputDriver::DidEditFieldInInsecureContext() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  // We aren't guaranteed to always have a navigation entry.
  if (!entry)
    return;

  content::SSLStatus& ssl = entry->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data = base::MakeUnique<security_state::SSLStatusInputEventData>();
    input_events = static_cast<security_state::SSLStatusInputEventData*>(
        ssl.user_data.get());
  }

  input_events->input_events()->insecure_field_edited = true;
  web_contents->DidChangeVisibleSecurityState();
}
