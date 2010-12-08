// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
#define CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
#pragma once

#include <string>

// Delegate interface used by PrefValueStore to notify its owner about changes
// to the preference values.
// TODO(mnissler, danno): Move this declaration to pref_value_store.h once we've
// cleaned up all public uses of this interface.
class PrefNotifier {
 public:
  virtual ~PrefNotifier() {}

  // Sends out a change notification for the preference identified by
  // |pref_name|.
  virtual void OnPreferenceChanged(const std::string& pref_name) = 0;

  // Broadcasts the intialization completed notification.
  virtual void OnInitializationCompleted() = 0;
};

#endif  // CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
