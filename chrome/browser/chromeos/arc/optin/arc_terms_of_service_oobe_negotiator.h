// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor_observer.h"

namespace chromeos {
class ArcTermsOfServiceScreenActor;
}

namespace arc {

// Handles the Terms-of-service agreement user action via OOBE OptIn UI.
class ArcTermsOfServiceOobeNegotiator
    : public ArcTermsOfServiceNegotiator,
      public chromeos::ArcTermsOfServiceScreenActorObserver {
 public:
  ArcTermsOfServiceOobeNegotiator();
  ~ArcTermsOfServiceOobeNegotiator() override;

  // Injects ARC OOBE screen handler in unit tests, where OOBE UI is not
  // available.
  static void SetArcTermsOfServiceScreenActorForTesting(
      chromeos::ArcTermsOfServiceScreenActor* actor);

 private:
  // Helper to handle callbacks from
  // chromeos::ArcTermsOfServiceScreenActorObserver. It removes observer from
  // |screen_actor_|, resets it, and then dispatches |accepted|. It is expected
  // that this method is called exactly once for each instance of
  // ArcTermsOfServiceOobeNegotiator.
  void HandleTermsAccepted(bool accepted);

  // chromeos::ArcTermsOfServiceScreenActorObserver:
  void OnSkip() override;
  void OnAccept() override;
  void OnActorDestroyed(chromeos::ArcTermsOfServiceScreenActor* actor) override;

  // ArcTermsOfServiceNegotiator:
  void StartNegotiationImpl() override;

  // Unowned pointer. If a user signs out while ARC OOBE opt-in is active,
  // LoginDisplayHost is detached first then OnActorDestroyed is called.
  // It means, in OnSkip() and OnAccept(), the Actor needs to be obtained via
  // LoginDisplayHost, but in OnActorDestroyed(), the argument needs to be used.
  // In order to use the same way to access the Actor, remember the pointer in
  // StartNegotiationImpl(), and reset in HandleTermsAccepted().
  chromeos::ArcTermsOfServiceScreenActor* screen_actor_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceOobeNegotiator);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_
