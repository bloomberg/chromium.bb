// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_menu_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#else
#include "content/public/browser/browser_accessibility_state.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

AccessibilityLabelsMenuObserver::AccessibilityLabelsMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

AccessibilityLabelsMenuObserver::~AccessibilityLabelsMenuObserver() {}

void AccessibilityLabelsMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  if (ShouldShowLabelsItem()) {
    proxy_->AddAccessibilityLabelsServiceItem(profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled));
  }
}

bool AccessibilityLabelsMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE;
}

bool AccessibilityLabelsMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());

  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) {
    return profile->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled);
  }
  return false;
}

bool AccessibilityLabelsMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));
  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) {
    return ShouldShowLabelsItem();
  }
  return false;
}

void AccessibilityLabelsMenuObserver::ExecuteCommand(int command_id) {
  // TODO(katie): Add logging.
  DCHECK(IsCommandIdSupported(command_id));

  Profile* profile = Profile::FromBrowserContext(proxy_->GetBrowserContext());
  if (command_id == IDC_CONTENT_CONTEXT_ACCESSIBILITY_LABELS_TOGGLE) {
    // When a user enables the accessibility labeling item, we
    // show a bubble to confirm it. On the other hand, when a user disables this
    // item, we directly update/ the profile and stop integrating the labels
    // service immediately.
    if (!profile->GetPrefs()->GetBoolean(
            prefs::kAccessibilityImageLabelsEnabled)) {
      content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
      gfx::Rect rect = rvh->GetWidget()->GetView()->GetViewBounds();
      std::unique_ptr<AccessibilityLabelsBubbleModel> model(
          new AccessibilityLabelsBubbleModel(profile,
                                             proxy_->GetWebContents()));
      chrome::ShowConfirmBubble(
          proxy_->GetWebContents()->GetTopLevelNativeWindow(),
          rvh->GetWidget()->GetView()->GetNativeView(),
          gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
    } else {
      if (profile) {
        profile->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                        false);
      }
    }
  }
}

bool AccessibilityLabelsMenuObserver::ShouldShowLabelsItem() {
  // Hidden behind a feature flag.
  base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  if (!cmd.HasSwitch(::switches::kEnableExperimentalAccessibilityLabels)) {
    return false;
  }

  // Check if a screen reader is running.
#if defined(OS_CHROMEOS)
  return chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
#else
  // TODO(katie): Can we use AXMode in Chrome OS as well?
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  return mode.has_mode(ui::AXMode::kScreenReader);
#endif  // defined(OS_CHROMEOS)
}
