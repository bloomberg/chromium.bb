// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"

class IdentityInternalsUITest;

// The WebUI for chrome://identity-internals
class IdentityInternalsUI : public MojoWebUIController {
 public:
  explicit IdentityInternalsUI(content::WebUI* web_ui);
  virtual ~IdentityInternalsUI();

 private:
  // MojoWebUIController overrides:
  virtual scoped_ptr<MojoWebUIHandler> CreateUIHandler(
      mojo::ScopedMessagePipeHandle handle_to_page) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(IdentityInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_IDENTITY_INTERNALS_UI_H_
