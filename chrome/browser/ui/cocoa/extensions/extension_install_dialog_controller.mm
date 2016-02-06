// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_install_dialog_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/experience_sampling_private/experience_sampling.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_window.h"
#import "chrome/browser/ui/cocoa/extensions/windowed_install_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

using extensions::ExperienceSamplingEvent;

namespace {

void ShowExtensionInstallDialogImpl(
    ExtensionInstallPromptShowParams* show_params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    scoped_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  // These objects will delete themselves when the dialog closes.
  if (!show_params->GetParentWebContents()) {
    new WindowedInstallDialogController(show_params, done_callback,
                                        std::move(prompt));
    return;
  }

  new ExtensionInstallDialogController(show_params, done_callback,
                                       std::move(prompt));
}

}  // namespace

ExtensionInstallDialogController::ExtensionInstallDialogController(
    ExtensionInstallPromptShowParams* show_params,
    const ExtensionInstallPrompt::DoneCallback& done_callback,
    scoped_ptr<ExtensionInstallPrompt::Prompt> prompt)
    : done_callback_(done_callback) {
  ExtensionInstallPrompt::PromptType promptType = prompt->type();
  view_controller_.reset([[ExtensionInstallViewController alloc]
      initWithProfile:show_params->profile()
            navigator:show_params->GetParentWebContents()
             delegate:this
               prompt:std::move(prompt)]);

  base::scoped_nsobject<NSWindow> window([[ConstrainedWindowCustomWindow alloc]
      initWithContentRect:[[view_controller_ view] bounds]]);
  [[window contentView] addSubview:[view_controller_ view]];

  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc] initWithCustomWindow:window]);
  constrained_window_ = CreateAndShowWebModalDialogMac(
      this, show_params->GetParentWebContents(), sheet);

  std::string event_name = ExperienceSamplingEvent::kExtensionInstallDialog;
  event_name.append(
      ExtensionInstallPrompt::PromptTypeToString(promptType));
  sampling_event_ = ExperienceSamplingEvent::Create(event_name);
}

ExtensionInstallDialogController::~ExtensionInstallDialogController() {
}

void ExtensionInstallDialogController::OnOkButtonClicked() {
  OnPromptButtonClicked(ExtensionInstallPrompt::Result::ACCEPTED,
                        ExperienceSamplingEvent::kProceed);
}

void ExtensionInstallDialogController::OnCancelButtonClicked() {
  OnPromptButtonClicked(ExtensionInstallPrompt::Result::USER_CANCELED,
                        ExperienceSamplingEvent::kDeny);
}

void ExtensionInstallDialogController::OnStoreLinkClicked() {
  OnPromptButtonClicked(ExtensionInstallPrompt::Result::USER_CANCELED,
                        ExperienceSamplingEvent::kDeny);
}

void ExtensionInstallDialogController::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  if (!done_callback_.is_null()) {
    base::ResetAndReturn(&done_callback_).Run(
        ExtensionInstallPrompt::Result::ABORTED);
  }
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ExtensionInstallDialogController::OnPromptButtonClicked(
    ExtensionInstallPrompt::Result result,
    const char* decision_event) {
  if (sampling_event_.get())
    sampling_event_->CreateUserDecisionEvent(decision_event);
  base::ResetAndReturn(&done_callback_).Run(result);
  constrained_window_->CloseWebContentsModalDialog();
}

// static
ExtensionInstallPrompt::ShowDialogCallback
ExtensionInstallPrompt::GetDefaultShowDialogCallback() {
  return base::Bind(&ShowExtensionInstallDialogImpl);
}
