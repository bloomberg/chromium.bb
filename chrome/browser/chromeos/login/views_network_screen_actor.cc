// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_network_screen_actor.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Considering 10px shadow from each side & welcome title height at 30px.
const int kWelcomeScreenWidth = 580;
const int kWelcomeScreenHeight = 335;

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// ViewsNetworkScreenActor, public:

ViewsNetworkScreenActor::ViewsNetworkScreenActor(
    ViewScreenDelegate* delegate)
    : ViewScreen<NetworkSelectionView>(
        delegate,
        kWelcomeScreenWidth,
        kWelcomeScreenHeight),
      bubble_(NULL),
      screen_(NULL) {
}

ViewsNetworkScreenActor::~ViewsNetworkScreenActor() {
  ClearErrors();
}

void ViewsNetworkScreenActor::SetDelegate(Delegate* screen) {
  screen_ = screen;
}

bool ViewsNetworkScreenActor::IsErrorShown() const {
  return bubble_ != NULL;
}

bool ViewsNetworkScreenActor::IsContinueEnabled() const {
  DCHECK(view());
  if (view())
    return view()->IsContinueEnabled();
  return false;
}

bool ViewsNetworkScreenActor::IsConnecting() const {
  DCHECK(view());
  if (view())
    return view()->IsConnecting();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ViewsNetworkScreenActor, NetworkScreenActor implementation:
void ViewsNetworkScreenActor::PrepareToShow() {
  ViewScreen<NetworkSelectionView>::PrepareToShow();
}

void ViewsNetworkScreenActor::Show() {
  ViewScreen<NetworkSelectionView>::Show();
}

void ViewsNetworkScreenActor::Hide() {
  ViewScreen<NetworkSelectionView>::Hide();
}

gfx::Size ViewsNetworkScreenActor::GetScreenSize() const {
  return ViewScreen<NetworkSelectionView>::GetScreenSize();
}

void ViewsNetworkScreenActor::ShowError(const string16& message) {
  if (!view()->is_dialog_open() &&
      !(help_app_.get() && help_app_->is_open())) {
    ClearErrors();
    views::View* network_control = view()->GetNetworkControlView();
    bubble_ = MessageBubble::Show(
        network_control->GetWidget(),
        network_control->GetScreenBounds(),
        BubbleBorder::LEFT_TOP,
        ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
        UTF16ToWide(message),
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_LEARN_MORE)),
        this);
    network_control->RequestFocus();
  }
}

void ViewsNetworkScreenActor::ClearErrors() {
  // bubble_ will be set to NULL in callback.
  if (bubble_)
    bubble_->Close();
}

void ViewsNetworkScreenActor::ShowConnectingStatus(
    bool connecting,
    const string16& network_id) {
  DCHECK(view());
  if (view())
    view()->ShowConnectingStatus(connecting, network_id);
}

void ViewsNetworkScreenActor::EnableContinue(bool enabled) {
  DCHECK(view());
  if (view())
    view()->EnableContinue(enabled);
}

///////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void ViewsNetworkScreenActor::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  ClearErrors();
  screen_->OnContinuePressed();
}

///////////////////////////////////////////////////////////////////////////////
// ViewsNetworkScreenActor, views::BubbleDelegate implementation:

void ViewsNetworkScreenActor::BubbleClosing(Bubble* bubble,
                                            bool closed_by_escape) {
  bubble_ = NULL;
}

bool ViewsNetworkScreenActor::CloseOnEscape() {
  return true;
}

bool ViewsNetworkScreenActor::FadeInOnShow() {
  return false;
}

void ViewsNetworkScreenActor::OnLinkActivated(size_t index) {
  ClearErrors();
  if (!help_app_.get()) {
    help_app_ = new HelpAppLauncher(
        LoginUtils::Get()->GetBackgroundView()->GetNativeWindow());
  }
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_CONNECTIVITY);
}

///////////////////////////////////////////////////////////////////////////////
// ViewsNetworkScreenActor, ViewScreen implementation:
void ViewsNetworkScreenActor::CreateView() {
  language_switch_menu_.InitLanguageMenu();
  ViewScreen<NetworkSelectionView>::CreateView();
}

NetworkSelectionView* ViewsNetworkScreenActor::AllocateView() {
  return new NetworkSelectionView(this);
}

}  // namespace chromeos
