// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/observer_list.h"
#include "ios/chrome/browser/overlays/public/overlay_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

class WebStateList;

// Internal implementation of OverlayManager.
class OverlayManagerImpl : public OverlayManager {
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
    explicit Container(WebStateList* web_state_list);

    WebStateList* web_state_list_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayManagerImpl>> managers_;
  };

  // OverlayManager:
  void SetUIDelegate(OverlayUIDelegate* ui_delegate) override;
  void AddObserver(OverlayManagerObserver* observer) override;
  void RemoveObserver(OverlayManagerObserver* observer) override;

 private:
  OverlayManagerImpl(WebStateList* web_state_list);

  // The manager's observers.
  base::ObserverList<OverlayManagerObserver>::Unchecked observers_;
  // The Browser's WebStateList.  Used to update overlay scheduling for when
  // WebStates are removed from the Browser.
  WebStateList* web_state_list_ = nullptr;
  // The UI delegate provided to the manager.
  OverlayUIDelegate* ui_delegate_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_MANAGER_IMPL_H_
