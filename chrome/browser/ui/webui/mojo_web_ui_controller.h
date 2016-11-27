// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/system/core.h"
#include "services/service_manager/public/cpp/interface_registry.h"

class MojoWebUIControllerBase : public content::WebUIController {
 public:
  explicit MojoWebUIControllerBase(content::WebUI* contents);
  ~MojoWebUIControllerBase() override;

  // WebUIController overrides:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoWebUIControllerBase);
};

// MojoWebUIController is intended for web ui pages that use mojo. It is
// expected that subclasses will do two things:
// . In the constructor invoke AddMojoResourcePath() to register the bindings
//   files, eg:
//     AddMojoResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
//                         IDR_OMNIBOX_MOJO_JS);
// . Override BindUIHandler() to create and bind the implementation of the
//   bindings.
template <typename Interface>
class MojoWebUIController : public MojoWebUIControllerBase {
 public:
  explicit MojoWebUIController(content::WebUI* contents)
      : MojoWebUIControllerBase(contents), weak_factory_(this) {}
  ~MojoWebUIController() override {}

  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    MojoWebUIControllerBase::RenderFrameCreated(render_frame_host);

    // Right now, this is expected to be called only for main frames.
    DCHECK(!render_frame_host->GetParent());
    render_frame_host->GetInterfaceRegistry()->
        AddInterface<Interface>(
            base::Bind(&MojoWebUIController::BindUIHandler,
                       weak_factory_.GetWeakPtr()));
  }

 protected:
  // Invoked to create the specific bindings implementation.
  virtual void BindUIHandler(mojo::InterfaceRequest<Interface> request) = 0;

 private:
  base::WeakPtrFactory<MojoWebUIController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoWebUIController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
