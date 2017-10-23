// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#include "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#import "chrome/browser/ui/cocoa/permission_bubble/permission_bubble_cocoa.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_impl.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

constexpr base::Feature kCocoaPermissionBubbles = {
    "CocoaPermissionBubbles", base::FEATURE_DISABLED_BY_DEFAULT};

bool UseViewsBubbles() {
  return chrome::ShowPilotDialogsWithViewsToolkit() ||
         !base::FeatureList::IsEnabled(kCocoaPermissionBubbles);
}

views::BubbleDialogDelegateView* BubbleForWindow(gfx::NativeWindow window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  return widget->widget_delegate()->AsBubbleDialogDelegate();
}

}  // namespace

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (UseViewsBubbles()) {
    auto prompt = base::MakeUnique<PermissionPromptImpl>(browser, delegate);
    // Note the PermissionPromptImpl constructor always shows the bubble, which
    // is necessary to call TrackBubbleState().
    // Also note it's important to use BrowserWindow::GetNativeWindow() and not
    // WebContents::GetTopLevelNativeWindow() below, since there's a brief
    // period when attaching a dragged tab to a window that WebContents thinks
    // it hasn't yet moved to the new window.
    TrackBubbleState(
        BubbleForWindow(prompt->GetNativeWindow()),
        GetPageInfoDecoration(browser->window()->GetNativeWindow()));
    return prompt;
  }
  return base::MakeUnique<PermissionBubbleCocoa>(browser, delegate);
}
