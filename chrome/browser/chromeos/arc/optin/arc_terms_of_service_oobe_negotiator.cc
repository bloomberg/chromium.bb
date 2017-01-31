// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_oobe_negotiator.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace arc {

namespace {

chromeos::ArcTermsOfServiceScreenActor* g_actor_for_testing = nullptr;

chromeos::ArcTermsOfServiceScreenActor* GetScreenActor() {
  // Inject testing instance.
  if (g_actor_for_testing)
    return g_actor_for_testing;

  chromeos::LoginDisplayHost* host = chromeos::LoginDisplayHost::default_host();
  DCHECK(host);
  DCHECK(host->GetOobeUI());
  return host->GetOobeUI()->GetArcTermsOfServiceScreenActor();
}

}  // namespace

// static
void ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenActorForTesting(
    chromeos::ArcTermsOfServiceScreenActor* actor) {
  g_actor_for_testing = actor;
}

ArcTermsOfServiceOobeNegotiator::ArcTermsOfServiceOobeNegotiator() = default;

ArcTermsOfServiceOobeNegotiator::~ArcTermsOfServiceOobeNegotiator() {
  DCHECK(!screen_actor_);
}

void ArcTermsOfServiceOobeNegotiator::StartNegotiationImpl() {
  DCHECK(!screen_actor_);
  screen_actor_ = GetScreenActor();
  DCHECK(screen_actor_);
  screen_actor_->AddObserver(this);
}

void ArcTermsOfServiceOobeNegotiator::HandleTermsAccepted(bool accepted) {
  DCHECK(screen_actor_);
  screen_actor_->RemoveObserver(this);
  screen_actor_ = nullptr;
  ReportResult(accepted);
}

void ArcTermsOfServiceOobeNegotiator::OnSkip() {
  HandleTermsAccepted(false);
}

void ArcTermsOfServiceOobeNegotiator::OnAccept() {
  HandleTermsAccepted(true);
}

void ArcTermsOfServiceOobeNegotiator::OnActorDestroyed(
    chromeos::ArcTermsOfServiceScreenActor* actor) {
  DCHECK_EQ(actor, screen_actor_);
  HandleTermsAccepted(false);
}

}  // namespace arc
