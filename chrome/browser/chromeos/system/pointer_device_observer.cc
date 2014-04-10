// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/pointer_device_observer.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_X11)
#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"
#endif

using content::BrowserThread;

namespace chromeos {
namespace system {

PointerDeviceObserver::PointerDeviceObserver()
    : weak_factory_(this) {
}

PointerDeviceObserver::~PointerDeviceObserver() {
#if defined(USE_X11)
  XInputHierarchyChangedEventListener::GetInstance()
      ->RemoveObserver(this);
#endif
}

void PointerDeviceObserver::Init() {
#if defined(USE_X11)
  XInputHierarchyChangedEventListener::GetInstance()
      ->AddObserver(this);
#endif
}

void PointerDeviceObserver::CheckDevices() {
  CheckMouseExists();
  CheckTouchpadExists();
}

void PointerDeviceObserver::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PointerDeviceObserver::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PointerDeviceObserver::DeviceHierarchyChanged() {
  CheckDevices();
}

void PointerDeviceObserver::CheckTouchpadExists() {
  InputDeviceSettings::Get()->TouchpadExists(
      base::Bind(&PointerDeviceObserver::OnTouchpadExists,
                 weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::CheckMouseExists() {
  InputDeviceSettings::Get()->MouseExists(
      base::Bind(&PointerDeviceObserver::OnMouseExists,
                 weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::OnTouchpadExists(bool exists) {
  FOR_EACH_OBSERVER(Observer, observers_, TouchpadExists(exists));
}

void PointerDeviceObserver::OnMouseExists(bool exists) {
  FOR_EACH_OBSERVER(Observer, observers_, MouseExists(exists));
}

PointerDeviceObserver::Observer::~Observer() {
}

}  // namespace system
}  // namespace chromeos
