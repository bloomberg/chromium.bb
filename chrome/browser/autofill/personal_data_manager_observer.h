// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_OBSERVER_H_
#pragma once

// An interface the PersonalDataManager uses to notify its clients (observers)
// when it has finished loading personal data from the web database.  Register
// the observer via PersonalDataManager::SetObserver.
class PersonalDataManagerObserver {
 public:
  // Notifies the observer that the PersonalDataManager changed in some way.
  virtual void OnPersonalDataChanged() = 0;
  virtual void OnInsufficientFormData() {}

 protected:
  virtual ~PersonalDataManagerObserver() {}
};

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_DATA_MANAGER_OBSERVER_H_
