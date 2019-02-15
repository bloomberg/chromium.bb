// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#if !defined(OS_CHROMEOS)
#include "content/public/browser/browser_accessibility_state.h"
#endif  // !defined(OS_CHROMEOS)

namespace settings {

AccessibilityMainHandler::AccessibilityMainHandler() {
#if defined(OS_CHROMEOS)
  accessibility_subscription_ =
      chromeos::AccessibilityManager::Get()->RegisterCallback(
          base::BindRepeating(
              &AccessibilityMainHandler::OnAccessibilityStatusChanged,
              base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
}

AccessibilityMainHandler::~AccessibilityMainHandler() {}

void AccessibilityMainHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getScreenReaderState",
      base::BindRepeating(&AccessibilityMainHandler::HandleGetScreenReaderState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "confirmA11yImageLabels",
      base::BindRepeating(
          &AccessibilityMainHandler::HandleCheckAccessibilityImageLabels,
          base::Unretained(this)));
}

void AccessibilityMainHandler::OnAXModeAdded(ui::AXMode mode) {
  HandleGetScreenReaderState(nullptr);
}

void AccessibilityMainHandler::HandleGetScreenReaderState(
    const base::ListValue* args) {
  base::Value result(accessibility_state_utils::IsScreenReaderEnabled());
  AllowJavascript();
  FireWebUIListener("screen-reader-state-changed", result);
}

void AccessibilityMainHandler::HandleCheckAccessibilityImageLabels(
    const base::ListValue* args) {
  // When the user tries to enable the feature, show the modal dialog. The
  // dialog will disable the feature again if it is not accepted.
  content::WebContents* web_contents = web_ui()->GetWebContents();
  content::RenderWidgetHostView* view =
      web_contents->GetRenderViewHost()->GetWidget()->GetView();
  gfx::Rect rect = view->GetViewBounds();
  auto model = std::make_unique<AccessibilityLabelsBubbleModel>(
      Profile::FromWebUI(web_ui()), web_contents, true /* enable always */);
  chrome::ShowConfirmBubble(
      web_contents->GetTopLevelNativeWindow(), view->GetNativeView(),
      gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
}

#if defined(OS_CHROMEOS)
void AccessibilityMainHandler::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    HandleGetScreenReaderState(nullptr);
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
