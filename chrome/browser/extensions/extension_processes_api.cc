// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_processes_api.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"

#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace keys = extension_processes_api_constants;

DictionaryValue* CreateProcessValue(int process_id,
                                    std::string type,
                                    double cpu,
                                    int net,
                                    int pr_mem,
                                    int sh_mem) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, process_id);
  result->SetString(keys::kTypeKey, type);
  result->SetReal(keys::kCpuKey, cpu);
  result->SetInteger(keys::kNetworkKey, net);
  result->SetInteger(keys::kPrivateMemoryKey, pr_mem);
  result->SetInteger(keys::kSharedMemoryKey, sh_mem);
  return result;
}

ExtensionProcessesEventRouter* ExtensionProcessesEventRouter::GetInstance() {
  return Singleton<ExtensionProcessesEventRouter>::get();
}

ExtensionProcessesEventRouter::ExtensionProcessesEventRouter() {
  model_ = TaskManager::GetInstance()->model();
  model_->AddObserver(this);
}

ExtensionProcessesEventRouter::~ExtensionProcessesEventRouter() {
  model_->RemoveObserver(this);
}

void ExtensionProcessesEventRouter::ObserveProfile(Profile* profile) {
  profiles_.insert(profile);
}

void ExtensionProcessesEventRouter::ListenerAdded() {
  model_->StartUpdating();
}

void ExtensionProcessesEventRouter::ListenerRemoved() {
  model_->StopUpdating();
}

void ExtensionProcessesEventRouter::OnItemsChanged(int start, int length) {
  if (model_) {
    ListValue args;
    DictionaryValue* processes = new DictionaryValue();
    for (int i = start; i < start + length; i++) {
      if (model_->IsResourceFirstInGroup(i)) {
        int id = model_->GetProcessId(i);
        std::string type = keys::kProcessTypePlugin;
        TabContents* contents = model_->GetResourceTabContents(i);
        if (contents) {
          type = keys::kProcessTypeRenderer;
        } else if (model_->GetResourceExtension(i)) {
          type = keys::kProcessTypeExtension;
        } else if (TaskManager::GetInstance()->IsBrowserProcess(i)) {
          type = keys::kProcessTypeBrowser;
        }

        // Get process metrics as numbers
        double cpu = model_->GetCPUUsage(i);
        // TODO(creis): Network is actually reported per-resource (tab),
        // not per-process.  We should aggregate it here.
        int net = (int) model_->GetNetworkUsage(i);
        size_t mem;
        int pr_mem = (model_->GetPrivateMemory(i, &mem) ? mem : -1);
        int sh_mem = (model_->GetSharedMemory(i, &mem) ? mem : -1);

        // Store each process indexed by the string version of its id
        processes->Set(base::IntToString(id),
                       CreateProcessValue(id, type, cpu, net, pr_mem, sh_mem));
      }
    }
    args.Append(processes);

    std::string json_args;
    base::JSONWriter::Write(&args, false, &json_args);

    // Notify each profile that is interested.
    for (ProfileSet::iterator it = profiles_.begin();
         it != profiles_.end(); it++) {
      Profile* profile = *it;
      DispatchEvent(profile, keys::kOnUpdated, json_args);
    }
  }
}

void ExtensionProcessesEventRouter::DispatchEvent(Profile* profile,
    const char* event_name,
    const std::string& json_args) {
  if (profile && profile->GetExtensionMessageService()) {
    profile->GetExtensionMessageService()->DispatchEventToRenderers(
        event_name, json_args, NULL, GURL());
  }
}

bool GetProcessForTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabContents* contents = NULL;
  int tab_index = -1;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile(), include_incognito(),
                                    NULL, NULL, &contents, &tab_index))
    return false;

  // Return the process ID of the tab as an integer.
  int id = base::GetProcId(contents->GetRenderProcessHost()->GetHandle());
  result_.reset(Value::CreateIntegerValue(id));
  return true;
}
