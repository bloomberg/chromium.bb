// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace infobars {
class InfoBar;
}

namespace web {
struct LoadCommittedDetails;
class WebState;
}

// Associates a Tab to an InfoBarManager and manages its lifetime.
// It responds to navigation events.
class InfoBarManagerImpl : public infobars::InfoBarManager,
                           public web::WebStateObserver,
                           public web::WebStateUserData<InfoBarManagerImpl> {
 public:
  // This function must only be called on infobars that are owned by an
  // InfoBarManagerImpl instance (or not owned at all, in which case this
  // returns null).
  static web::WebState* WebStateFromInfoBar(infobars::InfoBar* infobar);

 private:
  friend class web::WebStateUserData<InfoBarManagerImpl>;

  explicit InfoBarManagerImpl(web::WebState* web_state);
  ~InfoBarManagerImpl() override;

  // InfoBarManager implementation.
  int GetActiveEntryID() override;
  scoped_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate> delegate) override;

  // web::WebStateObserver implementation.
  void NavigationItemCommitted(
      const web::LoadCommittedDetails& load_details) override;
  void WebStateDestroyed() override;

  DISALLOW_COPY_AND_ASSIGN(InfoBarManagerImpl);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_MANAGER_IMPL_H_
