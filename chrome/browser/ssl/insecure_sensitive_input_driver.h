// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_H_
#define CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom.h"

namespace content {
class RenderFrameHost;
}

// The InsecureSensitiveInputDriver watches for SensitiveInputVisibility
// events from renderers, and surfaces notifications through
// VisiblePasswordObservers.
//
// There is one InsecureSensitiveInputDriver per RenderFrameHost.
// The lifetime is managed by the InsecureSensitiveInputDriverFactory.
class InsecureSensitiveInputDriver
    : public blink::mojom::SensitiveInputVisibilityService {
 public:
  explicit InsecureSensitiveInputDriver(
      content::RenderFrameHost* render_frame_host);
  ~InsecureSensitiveInputDriver() override;

  void BindSensitiveInputVisibilityServiceRequest(
      blink::mojom::SensitiveInputVisibilityServiceRequest request);

  // blink::mojom::SensitiveInputVisibility:
  void PasswordFieldVisibleInInsecureContext() override;
  void AllPasswordFieldsInInsecureContextInvisible() override;

 private:
  content::RenderFrameHost* render_frame_host_;

  mojo::BindingSet<blink::mojom::SensitiveInputVisibilityService>
      sensitive_input_visibility_bindings_;

  DISALLOW_COPY_AND_ASSIGN(InsecureSensitiveInputDriver);
};

#endif  // CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_H_
