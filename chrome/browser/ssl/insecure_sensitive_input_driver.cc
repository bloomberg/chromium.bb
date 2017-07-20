// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_sensitive_input_driver.h"

#include <utility>

#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "chrome/browser/ssl/visible_password_observer.h"
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

void InsecureSensitiveInputDriver::BindSensitiveInputVisibilityServiceRequest(
    blink::mojom::SensitiveInputVisibilityServiceRequest request) {
  sensitive_input_visibility_bindings_.AddBinding(this, std::move(request));
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
