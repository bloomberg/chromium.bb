// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor_observer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class ArcTermsOfServiceScreenActor;
class BaseScreenDelegate;

class ArcTermsOfServiceScreen : public BaseScreen,
                                public ArcTermsOfServiceScreenActorObserver {
 public:
  ArcTermsOfServiceScreen(BaseScreenDelegate* base_screen_delegate,
                          ArcTermsOfServiceScreenActor* actor);
  ~ArcTermsOfServiceScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // ArcTermsOfServiceScreenActorObserver:
  void OnSkip() override;
  void OnAccept() override;
  void OnActorDestroyed(ArcTermsOfServiceScreenActor* actor) override;

 private:
  ArcTermsOfServiceScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
