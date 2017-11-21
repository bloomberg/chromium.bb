// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_
#define CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_

// An interface to handle user actions assocated to an intervention.
class InterventionDelegate {
 public:
  virtual void AcceptIntervention() = 0;
  virtual void DeclineIntervention() = 0;

 protected:
  virtual ~InterventionDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_
