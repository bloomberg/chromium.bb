// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_OBSERVER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT LocaleObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void AcceptLocaleChange() = 0;
    virtual void RevertLocaleChange() = 0;
  };

  virtual ~LocaleObserver() {}

  virtual void OnLocaleChanged(Delegate* delegate,
                               const std::string& cur_locale,
                               const std::string& from_locale,
                               const std::string& to_locale) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_OBSERVER_H_
