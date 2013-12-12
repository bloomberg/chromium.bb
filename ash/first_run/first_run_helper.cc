// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper.h"

namespace ash {

FirstRunHelper::FirstRunHelper() {}
FirstRunHelper::~FirstRunHelper() {}

void FirstRunHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FirstRunHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos

