// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/constants/chromeos_switches.h"

namespace chromeos {
namespace supervision {

OnboardingControllerImpl::OnboardingControllerImpl() = default;
OnboardingControllerImpl::~OnboardingControllerImpl() = default;

void OnboardingControllerImpl::BindRequest(
    mojom::OnboardingControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OnboardingControllerImpl::BindWebviewHost(
    mojom::OnboardingWebviewHostPtr webview_host) {
  webview_host_ = std::move(webview_host);

  std::string start_page_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kSupervisionOnboardingStartPageUrl);
  if (start_page_url.empty()) {
    webview_host_->ExitFlow();
    return;
  }

  webview_host_->LoadPage(start_page_url);
}

void OnboardingControllerImpl::HandleAction(
    mojom::OnboardingFlowAction action) {
  DCHECK(webview_host_);
  switch (action) {
    // TODO(958985): Implement the full flow state machine.
    case mojom::OnboardingFlowAction::kSkipFlow:
    case mojom::OnboardingFlowAction::kShowNextPage:
    case mojom::OnboardingFlowAction::kShowPreviousPage:
      webview_host_->ExitFlow();
      return;
  }
}

}  // namespace supervision
}  // namespace chromeos
