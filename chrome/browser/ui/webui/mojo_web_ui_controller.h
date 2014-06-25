// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/system/core.h"

class MojoWebUIHandler;

namespace content {
class WebUIDataSource;
}

// MojoWebUIController is intended for web ui pages that use mojo. It is
// expected that subclasses will do two things:
// . In the constructor invoke AddMojoResourcePath() to register the bindings
//   files, eg:
//     AddMojoResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
//                         IDR_OMNIBOX_MOJO_JS);
// . Override CreateUIHandler() to create the implementation of the bindings.
class MojoWebUIController : public content::WebUIController {
 public:
  explicit MojoWebUIController(content::WebUI* contents);
  virtual ~MojoWebUIController();

  // WebUIController overrides:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

 protected:
  // Invoke to register mapping between binding file and resource id (IDR_...).
  void AddMojoResourcePath(const std::string& path, int resource_id);

  // Invoked to create the specific bindings implementation.
  virtual scoped_ptr<MojoWebUIHandler> CreateUIHandler(
      mojo::ScopedMessagePipeHandle handle_to_page) = 0;

 private:
  // Invoked in response to a connection from the renderer.
  void CreateAndStoreUIHandler(mojo::ScopedMessagePipeHandle handle);

  // Bindings files are registered here.
  content::WebUIDataSource* mojo_data_source_;

  scoped_ptr<MojoWebUIHandler> ui_handler_;

  base::WeakPtrFactory<MojoWebUIController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoWebUIController);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MOJO_WEB_UI_CONTROLLER_H_
