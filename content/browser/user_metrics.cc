// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/user_metrics.h"

#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

base::LazyInstance<std::vector<ActionCallback> > g_action_callbacks =
    LAZY_INSTANCE_INITIALIZER;

// Forward declare because of circular dependency.
void CallRecordOnUI(const std::string& action);

void Record(const char *action) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CallRecordOnUI, action));
    return;
  }

  for (size_t i = 0; i < g_action_callbacks.Get().size(); i++)
    g_action_callbacks.Get()[i].Run(action);
}

void CallRecordOnUI(const std::string& action) {
  Record(action.c_str());
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

}  // namespace content
