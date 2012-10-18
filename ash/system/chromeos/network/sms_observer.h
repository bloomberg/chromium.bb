// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_SMS_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_SMS_OBSERVER_H

namespace base {
class DictionaryValue;
}

namespace ash {

const char kSmsNumberKey[] = "number";
const char kSmsTextKey[] = "text";

class SmsObserver {
 public:
  virtual ~SmsObserver() {}

  virtual void AddMessage(const base::DictionaryValue& message) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_SMS_OBSERVER_H
