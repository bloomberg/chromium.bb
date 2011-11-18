// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_processes_api_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

namespace keys = extension_processes_api_constants;

DictionaryValue* CreateProcessValue(int process_id,
                                    const std::string& type,
                                    double cpu,
                                    int64 net,
                                    int64 pr_mem,
                                    int64 sh_mem) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(keys::kIdKey, process_id);
  result->SetString(keys::kTypeKey, type);
  result->SetDouble(keys::kCpuKey, cpu);
  result->SetDouble(keys::kNetworkKey, static_cast<double>(net));
  result->SetDouble(keys::kPrivateMemoryKey, static_cast<double>(pr_mem));
  result->SetDouble(keys::kSharedMemoryKey, static_cast<double>(sh_mem));
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

        // Determine process type
        std::string type = keys::kProcessTypeOther;
        TaskManager::Resource::Type resource_type = model_->GetResourceType(i);
        switch (resource_type) {
          case TaskManager::Resource::BROWSER:
            type = keys::kProcessTypeBrowser;
            break;
          case TaskManager::Resource::RENDERER:
            type = keys::kProcessTypeRenderer;
            break;
          case TaskManager::Resource::EXTENSION:
            type = keys::kProcessTypeExtension;
            break;
          case TaskManager::Resource::NOTIFICATION:
            type = keys::kProcessTypeNotification;
            break;
          case TaskManager::Resource::PLUGIN:
            type = keys::kProcessTypePlugin;
            break;
          case TaskManager::Resource::WORKER:
            type = keys::kProcessTypeWorker;
            break;
          case TaskManager::Resource::NACL:
            type = keys::kProcessTypeNacl;
            break;
          case TaskManager::Resource::UTILITY:
            type = keys::kProcessTypeUtility;
            break;
          case TaskManager::Resource::GPU:
            type = keys::kProcessTypeGPU;
            break;
          case TaskManager::Resource::PROFILE_IMPORT:
          case TaskManager::Resource::ZYGOTE:
          case TaskManager::Resource::SANDBOX_HELPER:
          case TaskManager::Resource::UNKNOWN:
            type = keys::kProcessTypeOther;
            break;
          default:
            NOTREACHED() << "Unknown resource type.";
        }

        // Get process metrics as numbers
        double cpu = model_->GetCPUUsage(i);

        // TODO(creis): Network is actually reported per-resource (tab),
        // not per-process.  We should aggregate it here.
        int64 net = model_->GetNetworkUsage(i);
        size_t mem;
        int64 pr_mem = model_->GetPrivateMemory(i, &mem) ?
            static_cast<int64>(mem) : -1;
        int64 sh_mem = model_->GetSharedMemory(i, &mem) ?
            static_cast<int64>(mem) : -1;

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
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, json_args, NULL, GURL());
  }
}

bool GetProcessIdForTabFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabContentsWrapper* contents = NULL;
  int tab_index = -1;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile(), include_incognito(),
                                    NULL, NULL, &contents, &tab_index)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        extension_tabs_module_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }

  // Return the process ID of the tab as an integer.
  int id = base::GetProcId(contents->tab_contents()->
      GetRenderProcessHost()->GetHandle());
  result_.reset(Value::CreateIntegerValue(id));
  return true;
}
