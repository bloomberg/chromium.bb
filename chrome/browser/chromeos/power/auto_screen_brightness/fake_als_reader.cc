// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

FakeAlsReader::FakeAlsReader() : weak_ptr_factory_(this) {}

FakeAlsReader::~FakeAlsReader() = default;

void FakeAlsReader::ReportAmbientLightUpdate(const int lux) {
  for (auto& observer : observers_)
    observer.OnAmbientLightUpdated(lux);
}

void FakeAlsReader::ReportReaderInitialized() {
  for (auto& observer : observers_)
    observer.OnAlsReaderInitialized(status_);
}

AlsReader::AlsInitStatus FakeAlsReader::GetInitStatus() const {
  return status_;
}

void FakeAlsReader::AddObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeAlsReader::RemoveObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

base::WeakPtr<FakeAlsReader> FakeAlsReader::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
