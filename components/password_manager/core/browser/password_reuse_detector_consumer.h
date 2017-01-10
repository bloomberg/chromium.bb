// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

namespace password_manager {

// Callback interface for receiving a password reuse event.
class PasswordReuseDetectorConsumer
    : public base::SupportsWeakPtr<PasswordReuseDetectorConsumer> {
 public:
  PasswordReuseDetectorConsumer();
  virtual ~PasswordReuseDetectorConsumer();

  // Called when a password reuse is found.
  // |saved_domain| is the domain on which |password| is saved.
  // |saved_passwords| is total number of passwords stored in Password Manager.
  // |number_matches| is a number of sites on which |password| is saved.
  virtual void OnReuseFound(const base::string16& password,
                            const std::string& saved_domain,
                            int saved_passwords,
                            int number_matches) = 0;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_CONSUMER_H_
