// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_H_

#include "base/basictypes.h"

// Interface for querying the EULA accepted state and receiving a notification
// when the EULA is accepted. This abstracts away the platform-specific logic
// for EULA notifications on different platforms.
class EulaAcceptedNotifier {
 public:
  // Observes EULA accepted state changes.
  class Observer {
   public:
    virtual void OnEulaAccepted() = 0;
  };

  EulaAcceptedNotifier();
  virtual ~EulaAcceptedNotifier();

  // Initializes this class with the given |observer|. Must be called before
  // the class is used.
  void Init(Observer* observer);

  // Returns true if the EULA has been accepted. If the EULA has not yet been
  // accepted, begins monitoring the EULA state and will notify the observer
  // once the EULA has been accepted.
  virtual bool IsEulaAccepted() = 0;

  // Factory method for this class.
  static EulaAcceptedNotifier* Create();

 protected:
  // Notifies the observer that the EULA has been updated, to be called by
  // platform-specific subclasses.
  void NotifyObserver();

 private:
  // Observer of the EULA accepted notification.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(EulaAcceptedNotifier);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_H_
