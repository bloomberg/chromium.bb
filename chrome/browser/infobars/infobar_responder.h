// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_

#include "base/macros.h"
#include "components/infobars/core/infobar_manager.h"

namespace infobars {
class InfoBar;
}

class ConfirmInfoBarDelegate;
class InfoBarService;

// Used by test code to asynchronously respond to the first infobar shown, which
// must have a ConfirmInfoBarDelegate. This can be used to ensure various
// interaction flows work correctly.
//
// The asynchronous response matches how real users will use the infobar.
class InfoBarResponder : public infobars::InfoBarManager::Observer {
 public:
  // If |should_accept| is true, the responder will asynchronously Accept() the
  // infobar; otherwise it will Cancel() it.
  InfoBarResponder(InfoBarService* infobar_service, bool should_accept);
  ~InfoBarResponder() override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;

 private:
  void Respond(ConfirmInfoBarDelegate* delegate);

  InfoBarService* infobar_service_;
  bool should_accept_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarResponder);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
