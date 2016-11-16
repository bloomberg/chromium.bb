// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class BaseScreenDelegate;

class ArcTermsOfServiceScreen : public BaseScreen,
                                public ArcTermsOfServiceScreenActor::Delegate {
 public:
  ArcTermsOfServiceScreen(BaseScreenDelegate* base_screen_delegate,
                          ArcTermsOfServiceScreenActor* actor);
  ~ArcTermsOfServiceScreen() override;

  // BaseScreen:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // ArcTermsOfServiceScreenActor::Delegate:
  void OnSkip() override;
  void OnAccept() override;
  void OnActorDestroyed(ArcTermsOfServiceScreenActor* actor) override;

 private:
  void ApplyTerms(bool accepted);

  ArcTermsOfServiceScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ARC_TERMS_OF_SERVICE_SCREEN_H_
