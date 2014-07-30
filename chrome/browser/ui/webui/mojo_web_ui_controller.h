// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/system/core.h"

class MojoWebUIHandler;

namespace content {
class WebUIDataSource;
}

class MojoWebUIControllerBase : public content::WebUIController {
 public:
  explicit MojoWebUIControllerBase(content::WebUI* contents);
  virtual ~MojoWebUIControllerBase();

  // WebUIController overrides:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

 protected:
  // Invoke to register mapping between binding file and resource id (IDR_...).
  void AddMojoResourcePath(const std::string& path, int resource_id);

 private:
  // Bindings files are registered here.
  content::WebUIDataSource* mojo_data_source_;

  DISALLOW_COPY_AND_ASSIGN(MojoWebUIControllerBase);
};

// MojoWebUIController is intended for web ui pages that use mojo. It is
// expected that subclasses will do two things:
// . In the constructor invoke AddMojoResourcePath() to register the bindings
//   files, eg:
//     AddMojoResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
//                         IDR_OMNIBOX_MOJO_JS);
// . Override CreateUIHandler() to create the implementation of the bindings.
template <typename Interface>
class MojoWebUIController : public MojoWebUIControllerBase {
 public:
  explicit MojoWebUIController(content::WebUI* contents)
      : MojoWebUIControllerBase(contents), weak_factory_(this) {}
  virtual ~MojoWebUIController() {}
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE {
    MojoWebUIControllerBase::RenderViewCreated(render_view_host);
    render_view_host->GetMainFrame()->GetServiceRegistry()->
        AddService<Interface>(
            base::Bind(&MojoWebUIController::CreateAndStoreUIHandler,
                       weak_factory_.GetWeakPtr()));
  }

 protected:
  // Invoked to create the specific bindings implementation.
  virtual scoped_ptr<MojoWebUIHandler> CreateUIHandler(
      mojo::InterfaceRequest<Interface> request) = 0;

 private:
  void CreateAndStoreUIHandler(mojo::InterfaceRequest<Interface> request) {
    ui_handler_ = CreateUIHandler(request.Pass());
  }

  scoped_ptr<MojoWebUIHandler> ui_handler_;

  base::WeakPtrFactory<MojoWebUIController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoWebUIController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
