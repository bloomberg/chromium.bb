// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#import "ios/chrome/browser/main/browser_observer.h"
#include "ios/chrome/browser/overlays/overlay_presenter.h"
#include "ios/chrome/browser/overlays/public/overlay_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

// Internal implementation of OverlayManager.
class OverlayManagerImpl : public BrowserObserver, public OverlayManager {
 public:
  ~OverlayManagerImpl() override;

  // Container that stores the managers for each modality.  Usage example:
  //
  // OverlayManagerImpl::Container::FromUserData(browser)->
  //     ManagerForModality(OverlayModality::kWebContentArea);
  class Container : public OverlayUserData<Container> {
   public:
    ~Container() override;
    // Returns the OverlayManagerImpl for |modality|.
    OverlayManagerImpl* ManagerForModality(OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    Browser* browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayManagerImpl>> managers_;
  };

  // BrowserObserver:
  void BrowserDestroyed(Browser* browser) override;

  // OverlayManager:
  void SetUIDelegate(OverlayUIDelegate* ui_delegate) override;
  void AddObserver(OverlayManagerObserver* observer) override;
  void RemoveObserver(OverlayManagerObserver* observer) override;

 private:
  OverlayManagerImpl(Browser* browser, OverlayModality modality);

  base::ObserverList<OverlayManagerObserver>::Unchecked observers_;
  OverlayPresenter presenter_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_
