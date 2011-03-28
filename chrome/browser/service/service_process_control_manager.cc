// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/service/service_process_control_manager.h"

#include "base/memory/singleton.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control.h"
#include "content/browser/browser_thread.h"

ServiceProcessControlManager::ServiceProcessControlManager() {
}

ServiceProcessControlManager::~ServiceProcessControlManager() {
  DCHECK(process_control_list_.empty());
}

ServiceProcessControl* ServiceProcessControlManager::GetProcessControl(
    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(hclam): We will have different service process for different types of
  // service, but now we only create a new process for a different profile.
  for (ServiceProcessControlList::iterator i = process_control_list_.begin();
       i != process_control_list_.end(); ++i) {
    if ((*i)->profile() == profile)
      return *i;
  }

  // Couldn't find a ServiceProcess so construct a new one.
  ServiceProcessControl* process  = new ServiceProcessControl(profile);
  process_control_list_.push_back(process);
  return process;
}

void ServiceProcessControlManager::Shutdown() {
  // When we shutdown the manager we just clear the list and don't actually
  // shutdown the service process.
  STLDeleteElements(&process_control_list_);
}

// static
ServiceProcessControlManager* ServiceProcessControlManager::GetInstance() {
  return Singleton<ServiceProcessControlManager>::get();
}
