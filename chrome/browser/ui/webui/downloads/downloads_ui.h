// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/layout.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class RefCountedMemory;
}

class DownloadsDOMHandler;

class DownloadsUI : public ui::MojoWebUIController,
                    public downloads::mojom::PageHandlerFactory {
 public:
  explicit DownloadsUI(content::WebUI* web_ui);
  ~DownloadsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  void BindPageHandlerFactory(
      downloads::mojom::PageHandlerFactoryRequest request);

  // downloads::mojom::PageHandlerFactory:
  void CreatePageHandler(downloads::mojom::PagePtr page,
                         downloads::mojom::PageHandlerRequest request) override;

  std::unique_ptr<DownloadsDOMHandler> page_handler_;

  mojo::Binding<downloads::mojom::PageHandlerFactory> page_factory_binding_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_UI_H_
