// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_activity_log.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"

ExtensionActivityLog::ExtensionActivityLog() {
  log_activity_to_stdout_ = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityLogging);
}

ExtensionActivityLog::~ExtensionActivityLog() {
}

// static
ExtensionActivityLog* ExtensionActivityLog::GetInstance() {
  return Singleton<ExtensionActivityLog>::get();
}

void ExtensionActivityLog::AddObserver(
    const extensions::Extension* extension,
    ExtensionActivityLog::Observer* observer) {
  base::AutoLock scoped_lock(lock_);

  if (observers_.count(extension) == 0) {
    observers_[extension] = new ObserverListThreadSafe<Observer>;
  }

  observers_[extension]->AddObserver(observer);
}

void ExtensionActivityLog::RemoveObserver(
    const extensions::Extension* extension,
    ExtensionActivityLog::Observer* observer) {
  base::AutoLock scoped_lock(lock_);

  if (observers_.count(extension) == 1) {
    observers_[extension]->RemoveObserver(observer);
  }
}
// Extension*
bool ExtensionActivityLog::HasObservers(
    const extensions::Extension* extension) const {
  base::AutoLock scoped_lock(lock_);

  // We also return true if extension activity logging is enabled since in that
  // case this class is observing all extensions.
  return observers_.count(extension) > 0 || log_activity_to_stdout_;
}

void ExtensionActivityLog::Log(const extensions::Extension* extension,
                               Activity activity,
                               const std::string& msg) const {
  base::AutoLock scoped_lock(lock_);

  ObserverMap::const_iterator iter = observers_.find(extension);
  if (iter != observers_.end()) {
    iter->second->Notify(&Observer::OnExtensionActivity, extension, activity,
                         msg);
  }

  if (log_activity_to_stdout_) {
    LOG(INFO) << extension->id() + ":" + ActivityToString(activity) + ":" + msg;
  }
}

// static
const char* ExtensionActivityLog::ActivityToString(Activity activity) {
  switch (activity) {
    case ExtensionActivityLog::ACTIVITY_EXTENSION_API_CALL:
      return "api_call";
    case ExtensionActivityLog::ACTIVITY_EXTENSION_API_BLOCK:
      return "api_block";
    default:
      NOTREACHED();
      return "";
  }
}
