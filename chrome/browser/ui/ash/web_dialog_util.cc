// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/web_dialog_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowWebDialogWithContainer(gfx::NativeView parent,
                                int container_id,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate) {
  DCHECK(parent || container_id != ash::kShellWindowId_Invalid);
  views::WebDialogView* view =
      new views::WebDialogView(context, delegate, new ChromeWebContentsHandler);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.delegate = view;
  if (chrome::IsRunningInMash()) {
    if (parent) {
      ui::Window* parent_mus = aura::GetMusWindow(parent);
      DCHECK(parent_mus);
      params.parent_mus = parent_mus;
    } else {
      using ui::mojom::WindowManager;
      params.mus_properties[WindowManager::kInitialContainerId_Property] =
          mojo::ConvertTo<std::vector<uint8_t>>(container_id);
    }
  } else {
    if (parent) {
      params.parent = parent;
    } else {
      params.parent = ash::Shell::GetContainer(
          ash::Shell::GetPrimaryRootWindow(), container_id);
    }
  }
  widget->Init(params);

  // Observer is needed for ChromeVox extension to send messages between content
  // and background scripts.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      view->web_contents());

  widget->Show();
}

}  // namespace chrome
