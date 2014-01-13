// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/user_metrics.h"

#include <vector>

#include "base/lazy_instance.h"

namespace base {
namespace {

base::LazyInstance<std::vector<ActionCallback> > g_action_callbacks =
    LAZY_INSTANCE_INITIALIZER;

void Record(const char *action) {
  for (size_t i = 0; i < g_action_callbacks.Get().size(); i++)
    g_action_callbacks.Get()[i].Run(action);
}

}  // namespace

void RecordAction(const UserMetricsAction& action) {
  Record(action.str_);
}

void RecordComputedAction(const std::string& action) {
  Record(action.c_str());
}

void AddActionCallback(const ActionCallback& callback) {
  g_action_callbacks.Get().push_back(callback);
}

void RemoveActionCallback(const ActionCallback& callback) {
  for (size_t i = 0; i < g_action_callbacks.Get().size(); i++) {
    if (g_action_callbacks.Get()[i].Equals(callback)) {
      g_action_callbacks.Get().erase(g_action_callbacks.Get().begin() + i);
      return;
    }
  }
}

}  // namespace base
