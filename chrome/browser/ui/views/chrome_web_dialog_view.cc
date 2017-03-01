// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/public/cpp/shell_window_ids.h"  // nogncheck
#include "ash/shell.h"                        // nogncheck
#include "chrome/browser/ui/ash/ash_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#endif  // defined(USE_ASH)

namespace chrome {
namespace {

gfx::NativeWindow ShowWebDialogWidget(const views::Widget::InitParams& params,
                                      views::WebDialogView* view) {
  views::Widget* widget = new views::Widget;
  widget->Init(params);

  // Observer is needed for ChromeVox extension to send messages between content
  // and background scripts.
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      view->web_contents());

  widget->Show();
  return widget->GetNativeWindow();
}

}  // namespace

// Declared in browser_dialogs.h so that others don't need to depend on our .h.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate) {
  views::WebDialogView* view =
      new views::WebDialogView(context, delegate, new ChromeWebContentsHandler);
  views::Widget::InitParams params;
  params.delegate = view;
  // NOTE: The |parent| may be null, which will result in the default window
  // placement on Aura.
  params.parent = parent;
  return ShowWebDialogWidget(params, view);
}

#if defined(USE_ASH)
void ShowWebDialogInContainer(int container_id,
                              content::BrowserContext* context,
                              ui::WebDialogDelegate* delegate) {
  DCHECK(container_id != ash::kShellWindowId_Invalid);
  views::WebDialogView* view =
      new views::WebDialogView(context, delegate, new ChromeWebContentsHandler);
  views::Widget::InitParams params;
  params.delegate = view;
  if (ash_util::IsRunningInMash()) {
    using ui::mojom::WindowManager;
    params.mus_properties[WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(container_id);
  } else {
    params.parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                             container_id);
  }
  ShowWebDialogWidget(params, view);
}
#endif  // defined(USE_ASH)

}  // namespace chrome
