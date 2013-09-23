// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_H_

#include "content/public/browser/web_ui_controller.h"

namespace dom_distiller {

// The WebUI handler for chrome://dom-distiller.
class DomDistillerUI : public content::WebUIController {
 public:
  explicit DomDistillerUI(content::WebUI* web_ui);
  virtual ~DomDistillerUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(DomDistillerUI);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_WEBUI_DOM_DISTILLER_UI_H_
