// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTER_UI_DELEGATE_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTER_UI_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_user_data.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_ui_state.h"
#import "ios/chrome/browser/ui/overlays/overlay_ui_dismissal_delegate.h"

@class OverlayRequestCoordinatorFactory;
@class OverlayContainerCoordinator;

// Implementation of OverlayPresenter::UIDelegate.  An instance of this class
// exists for every OverlayModality for each Browser.  This delegate is scoped
// to the Browser because it needs to store state even when a Browser's UI is
// not on screen.  When a Browser's UI is shown, the OverlayContainerCoordinator
// for each of its OverlayModalities will supply itself to the delegate, which
// will then present the UI using the container coordinator's presentation
// context.
class OverlayPresenterUIDelegateImpl : public OverlayPresenter::UIDelegate {
 public:
  ~OverlayPresenterUIDelegateImpl() override;

  // Container that stores the UI delegate for each modality.  Usage example:
  //
  // OverlayPresenterUIDelegateImpl::Container::FromUserData(browser)->
  //     UIDelegateForModality(OverlayModality::kWebContentArea);
  class Container : public OverlayUserData<Container> {
   public:
    ~Container() override;

    // Returns the OverlayPresenterUIDelegateImpl for |modality|.
    OverlayPresenterUIDelegateImpl* UIDelegateForModality(
        OverlayModality modality);

   private:
    OVERLAY_USER_DATA_SETUP(Container);
    explicit Container(Browser* browser);

    Browser* browser_ = nullptr;
    std::map<OverlayModality, std::unique_ptr<OverlayPresenterUIDelegateImpl>>
        ui_delegates_;
  };

  // The OverlayContainerCoordinator is used to present the overlay UI at the
  // correct modality in the app.  Should only be set when the coordinator's
  // presentation context is able to present.
  OverlayContainerCoordinator* coordinator() const { return coordinator_; }
  void SetCoordinator(OverlayContainerCoordinator* coordinator);

  // OverlayPresenter::UIDelegate:
  void ShowOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request,
                     OverlayDismissalCallback dismissal_callback) override;
  void HideOverlayUI(OverlayPresenter* presenter,
                     OverlayRequest* request) override;
  void CancelOverlayUI(OverlayPresenter* presenter,
                       OverlayRequest* request) override;

 private:
  OverlayPresenterUIDelegateImpl(Browser* browser, OverlayModality modality);

  // Setter for |request_|.  Setting to a new value will attempt to
  // present the UI for |request|.
  void SetRequest(OverlayRequest* request);

  // Returns the UI state for |request|.
  OverlayRequestUIState* GetRequestUIState(OverlayRequest* request);

  // Shows the UI for the presented request using the container coordinator.
  void ShowUIForPresentedRequest();

  // Dismisses the UI for the presented request for |reason|.
  void DismissPresentedUI(OverlayDismissalReason reason);

  // Called when the UI for |request_| has finished being dismissed.
  void OverlayUIWasDismissed();

  // Notifies the state for |request_| that its UI has finished being dismissed.
  void NotifyStateOfDismissal();

  // Helper object that detaches the UI delegate for Browser shudown.
  class BrowserShutdownHelper : public BrowserObserver {
   public:
    BrowserShutdownHelper(Browser* browser, OverlayPresenter* presenter);
    ~BrowserShutdownHelper() override;

    // BrowserObserver:
    void BrowserDestroyed(Browser* browser) override;

   private:
    // The presenter whose delegate needs to be reset.
    OverlayPresenter* presenter_ = nullptr;
  };

  // Helper object that listens for UI dismissal events.
  class OverlayDismissalHelper : public OverlayUIDismissalDelegate {
   public:
    OverlayDismissalHelper(OverlayPresenterUIDelegateImpl* ui_delegate);
    ~OverlayDismissalHelper() override;

    // OverlayUIDismissalDelegate:
    void OverlayUIDidFinishDismissal(OverlayRequest* request) override;

   private:
    OverlayPresenterUIDelegateImpl* ui_delegate_ = nullptr;
  };

  // The presenter whose UI is being handled by this delegate.
  OverlayPresenter* presenter_ = nullptr;
  // The cleanup helper.
  BrowserShutdownHelper shutdown_helper_;
  // The UI dismissal helper.
  OverlayDismissalHelper ui_dismissal_helper_;
  // The coordinator factory that provides the UI for the overlays at this
  // modality.
  OverlayRequestCoordinatorFactory* coordinator_factory_ = nil;
  // The coordinator responsible for presenting the UI delegate's UI.
  OverlayContainerCoordinator* coordinator_ = nil;
  // The request that is currently presented by |presenter_|.  The UI for this
  // request might not yet be visible if no OverlayContainerCoordinator has been
  // provided.  When a new request is presented, the UI state for the request
  // will be added to |states_|.
  OverlayRequest* request_ = nullptr;
  // Map storing the UI state for each OverlayRequest.
  std::map<OverlayRequest*, std::unique_ptr<OverlayRequestUIState>> states_;
  // Weak pointer factory.
  base::WeakPtrFactory<OverlayPresenterUIDelegateImpl> weak_factory_;
};

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTER_UI_DELEGATE_IMPL_H_
