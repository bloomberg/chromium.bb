// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/initializer.h"

namespace chromeos {

namespace tether {

Initializer::Initializer() {}

Initializer::~Initializer() {}

void Initializer::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void Initializer::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void Initializer::TransitionToStatus(Status new_status) {
  if (status_ == new_status)
    return;

  status_ = new_status;

  if (status_ != Status::SHUT_DOWN)
    return;

  for (auto& observer : observer_list_)
    observer.OnShutdownComplete();
}

}  // namespace tether

}  // namespace chromeos
