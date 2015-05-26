// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_ACTIVE_STATE_MANAGER_IMPL_H_
#define IOS_WEB_ACTIVE_STATE_MANAGER_IMPL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "ios/web/public/active_state_manager.h"

namespace web {

class BrowserState;

// Concrete subclass of web::ActiveStateManager. Informs observers when an
// ActiveStateManager becomes active/inactive.
class ActiveStateManagerImpl : public ActiveStateManager,
                               public base::SupportsUserData::Data {
 public:
  explicit ActiveStateManagerImpl(BrowserState* browser_state);
  ~ActiveStateManagerImpl() override;

  // ActiveStateManager methods.
  void SetActive(bool active) override;
  bool IsActive() override;

  // Observer that is notified when a ActiveStateManager becomes active,
  // inactive or destroyed.
  class Observer {
   public:
    // Called when the ActiveStateManager becomes active.
    virtual void OnActive() = 0;
    // Called when the ActiveStateManager becomes inactive.
    virtual void OnInactive() = 0;
    // Called just before the ActiveStateManager is destroyed.
    virtual void WillBeDestroyed() = 0;
  };
  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  void AddObserver(Observer* obs);
  // Removes an observer.
  void RemoveObserver(Observer* obs);

 private:
  BrowserState* browser_state_;  // weak, owns this object.
  // true if the ActiveStateManager is active.
  bool active_;
  // The list of observers.
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ActiveStateManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_ACTIVE_STATE_MANAGER_IMPL_H_
