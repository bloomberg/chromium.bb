// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"

#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

gfx::NativeWindow GetBrowserParentWindow() {
  if (chromeos::LoginDisplayHost::default_host())
    return chromeos::LoginDisplayHost::default_host()->GetNativeWindow();

  Browser* browser =
      chrome::FindTabbedBrowser(ProfileManager::GetPrimaryUserProfile(), true);
  if (browser)
    return browser->window()->GetNativeWindow();

  return nullptr;
}

}  // namespace

namespace chromeos {

// Define timeout for issued sign_request_id.
constexpr base::TimeDelta kSignRequestIdTimeout =
    base::TimeDelta::FromMinutes(10);

PinDialogManager::PinDialogManager() = default;

PinDialogManager::~PinDialogManager() {
  // Close the active dialog if present to avoid leaking callbacks.
  if (active_pin_dialog_)
    CloseDialog(active_dialog_extension_id_);
}

void PinDialogManager::AddSignRequestId(const std::string& extension_id,
                                        int sign_request_id) {
  ExtensionNameRequestIdPair key(extension_id, sign_request_id);
  // Cache the ID with current timestamp.
  sign_request_times_[key] = base::Time::Now();
}

PinDialogManager::RequestPinResult PinDialogManager::RequestPin(
    const std::string& extension_id,
    const std::string& extension_name,
    int sign_request_id,
    RequestPinView::RequestPinCodeType code_type,
    RequestPinView::RequestPinErrorType error_type,
    int attempts_left,
    RequestPinCallback callback) {
  bool accept_input = (attempts_left != 0);
  // If active dialog exists already, we need to make sure it belongs to the
  // same extension and the user submitted some input.
  if (active_pin_dialog_ != nullptr) {
    DCHECK(!active_dialog_extension_id_.empty());
    if (extension_id != active_dialog_extension_id_)
      return RequestPinResult::kOtherFlowInProgress;

    // Extension requests a PIN without having received any input from its
    // previous request. Reject the new request.
    if (!active_pin_dialog_->IsLocked())
      return RequestPinResult::kDialogDisplayedAlready;

    DCHECK(!active_request_pin_callback_);
    active_request_pin_callback_ = std::move(callback);

    active_pin_dialog_->SetDialogParameters(code_type, error_type,
                                            attempts_left, accept_input);
    active_pin_dialog_->DialogModelChanged();
    return RequestPinResult::kSuccess;
  }

  // Check the validity of sign_request_id
  const ExtensionNameRequestIdPair key(extension_id, sign_request_id);
  if (sign_request_times_.find(key) == sign_request_times_.end())
    return RequestPinResult::kInvalidId;

  const base::Time current_time = base::Time::Now();
  if (current_time - sign_request_times_[key] > kSignRequestIdTimeout)
    return RequestPinResult::kInvalidId;

  DCHECK(!active_request_pin_callback_);
  active_request_pin_callback_ = std::move(callback);

  active_dialog_extension_id_ = extension_id;
  active_pin_dialog_ =
      new RequestPinView(extension_name, code_type, attempts_left,
                         base::BindRepeating(&PinDialogManager::OnPinEntered,
                                             weak_factory_.GetWeakPtr()),
                         base::BindOnce(&PinDialogManager::OnViewDestroyed,
                                        weak_factory_.GetWeakPtr()));

  const gfx::NativeWindow parent = GetBrowserParentWindow();
  // If there is no parent, falls back to the root window for new windows.
  active_window_ = views::DialogDelegate::CreateDialogWidget(
      active_pin_dialog_, /*context=*/ nullptr, parent);
  active_window_->Show();

  return RequestPinResult::kSuccess;
}

PinDialogManager::StopPinRequestResult
PinDialogManager::StopPinRequestWithError(
    const std::string& extension_id,
    RequestPinView::RequestPinErrorType error_type,
    StopPinRequestCallback callback) {
  if (active_pin_dialog_ == nullptr ||
      extension_id != active_dialog_extension_id_) {
    return StopPinRequestResult::kNoActiveDialog;
  }

  if (!active_pin_dialog_->IsLocked())
    return StopPinRequestResult::kNoUserInput;

  DCHECK(!active_stop_pin_request_callback_);
  active_stop_pin_request_callback_ = std::move(callback);
  active_pin_dialog_->SetDialogParameters(
      RequestPinView::RequestPinCodeType::UNCHANGED, error_type,
      /*attempts_left=*/-1,
      /*accept_input=*/false);
  active_pin_dialog_->DialogModelChanged();
  return StopPinRequestResult::kSuccess;
}

bool PinDialogManager::LastPinDialogClosed(
    const std::string& extension_id) const {
  auto iter = last_response_closed_.find(extension_id);
  return iter != last_response_closed_.end() && iter->second;
}

bool PinDialogManager::CloseDialog(const std::string& extension_id) {
  if (extension_id != active_dialog_extension_id_ ||
      active_pin_dialog_ == nullptr) {
    LOG(ERROR) << "StopPinRequest called by unexpected extension: "
               << extension_id;
    return false;
  }

  // The view destruction may happen asynchronously after the Close() call. For
  // simplicity, clear our state and execute the callback immediately.
  active_window_->Close();
  if (active_pin_dialog_) {
    weak_factory_.InvalidateWeakPtrs();
    OnPinDialogClosed();
  }
  return true;
}

void PinDialogManager::ExtensionUnloaded(const std::string& extension_id) {
  if (active_pin_dialog_ && active_dialog_extension_id_ == extension_id)
    CloseDialog(extension_id);

  last_response_closed_[extension_id] = false;

  for (auto it = sign_request_times_.cbegin();
       it != sign_request_times_.cend();) {
    if (it->first.first == extension_id)
      sign_request_times_.erase(it++);
    else
      ++it;
  }
}

void PinDialogManager::OnPinEntered(const std::string& user_input) {
  DCHECK(!active_stop_pin_request_callback_);
  last_response_closed_[active_dialog_extension_id_] = false;
  if (active_request_pin_callback_)
    std::move(active_request_pin_callback_).Run(user_input);
}

void PinDialogManager::OnViewDestroyed() {
  OnPinDialogClosed();
}

void PinDialogManager::OnPinDialogClosed() {
  DCHECK(active_pin_dialog_);
  DCHECK(!active_request_pin_callback_ || !active_stop_pin_request_callback_);

  active_pin_dialog_ = nullptr;
  active_window_ = nullptr;
  last_response_closed_[active_dialog_extension_id_] = true;
  active_dialog_extension_id_.clear();
  if (active_request_pin_callback_)
    std::move(active_request_pin_callback_).Run(/*user_input=*/std::string());
  if (active_stop_pin_request_callback_)
    std::move(active_stop_pin_request_callback_).Run();
}

}  // namespace chromeos
