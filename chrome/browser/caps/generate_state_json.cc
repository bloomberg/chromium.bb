// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/caps/generate_state_json.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/task_manager.h"

namespace {

std::string Key(base::ProcessId pid, const char* category) {
  return category ?
      base::StringPrintf("process.%d.%s", pid, category) :
      base::StringPrintf("process.%d", pid);
}

using MemoryFn1 = bool (TaskManagerModel::*)(
    int index, size_t* result1) const;
using MemoryFn2 = bool (TaskManagerModel::*)(
    int index, size_t* result1, bool*) const;

int InMBFromB(size_t result_in_bytes) {
  return static_cast<int>(result_in_bytes / (1024 * 1024));
}

int InMBFromB(const TaskManagerModel* model, MemoryFn1 mfn, int index) {
  size_t result_in_bytes = 0;
  bool res = (model->*mfn)(index, &result_in_bytes);
  return res ? InMBFromB(result_in_bytes) : 0;
}

int InMBFromB(const TaskManagerModel* model, MemoryFn2 mfn, int index) {
  size_t result_in_bytes = 0;
  bool ignored;
  bool res = (model->*mfn)(index, &result_in_bytes, &ignored);
  return res ? InMBFromB(result_in_bytes) : 0;
}

class TaskManagerDataDumper :
    public base::RefCountedThreadSafe<TaskManagerDataDumper> {
 public:
  TaskManagerDataDumper(scoped_refptr<TaskManagerModel> model, base::File file)
      : model_(model), file_(std::move(file)) {
    model_->RegisterOnDataReadyCallback(
        base::Bind(&TaskManagerDataDumper::OnDataReady, this));
    model->StartListening();
    // Note that GenerateStateJSON 'new's this object which is reference
    // counted.
    AddRef();
  }

 private:
  friend class base::RefCountedThreadSafe<TaskManagerDataDumper>;
  ~TaskManagerDataDumper() {
  }

  void OnDataReady() {
    // Some data (for example V8 memory) has not yet arrived, so we wait.
    // TODO(cpu): Figure out how to make this reliable.
    static base::TimeDelta delay = base::TimeDelta::FromMilliseconds(250);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&TaskManagerDataDumper::OnDataReadyDelayed, this),
        delay);
  }

  void OnDataReadyDelayed() {
    model_->StopListening();
    // Task manager finally computed (most) values. Lets generate the JSON
    // data and send it over the pipe.
    base::DictionaryValue dict;
    GatherComputerValues(&dict);
    GatherChromeValues(&dict);

    std::string json;
    auto options = base::JSONWriter::OPTIONS_PRETTY_PRINT;
    if (!base::JSONWriter::WriteWithOptions(dict, options, &json))
      return;

    file_.WriteAtCurrentPos(json.c_str(), json.size());
    file_.Close();
    // this Release() causes our destruction.
    Release();
  }

 private:
  // TODO(cpu): split the key names below into a separate header that both
  // caps and chrome can use.

  void GatherComputerValues(base::DictionaryValue* dict) {
    base::CPU cpu;
    dict->SetInteger("system.cpu.type", cpu.type());
    dict->SetInteger("system.cpu.family", cpu.family());
    dict->SetInteger("system.cpu.model", cpu.model());
    dict->SetInteger("system.cpu.stepping", cpu.stepping());
    dict->SetString("system.cpu.brand", cpu.cpu_brand());
    dict->SetInteger("system.cpu.logicalprocessors",
                     base::SysInfo::NumberOfProcessors());
    dict->SetInteger("system.cpu.logicalprocessors",
                     base::SysInfo::NumberOfProcessors());
    int64_t memory = base::SysInfo::AmountOfPhysicalMemory();
    dict->SetInteger("system.memory.physical", InMBFromB(memory));
    memory = base::SysInfo::AmountOfAvailablePhysicalMemory();
    dict->SetInteger("system.memory.available", InMBFromB(memory));
    dict->SetInteger("system.uptime", base::SysInfo::Uptime().InSeconds());
    dict->SetString("os.name", base::SysInfo::OperatingSystemName());
#if !defined(OS_LINUX)
    int32_t major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    dict->SetInteger("os.version.major", major);
    dict->SetInteger("os.version.minor", minor);
    dict->SetInteger("os.version.bugfix", bugfix);
    dict->SetString("os.arch", base::SysInfo::OperatingSystemArchitecture());
#endif
  }

  void GatherChromeValues(base::DictionaryValue* dict) {
    for (int index = 0; index != model_->ResourceCount(); ++index) {
      auto pid = model_->GetProcessId(index);
      auto tabs_key =  Key(pid, "tabs");
      base::ListValue* tabs;
      if (!dict->GetList(tabs_key, &tabs)) {
        tabs = new base::ListValue;
        dict->Set(tabs_key, tabs);
        tabs->AppendString(model_->GetResourceTitle(index));

        dict->SetInteger(Key(pid, "memory.physical"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetPhysicalMemory, index));
        dict->SetInteger(Key(pid, "memory.private"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetPrivateMemory, index));
        dict->SetInteger(Key(pid, "memory.shared"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetSharedMemory, index));
        dict->SetInteger(Key(pid, "memory.video"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetVideoMemory, index));
        dict->SetInteger(Key(pid, "memory.V8.total"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetV8Memory, index));
        dict->SetInteger(Key(pid, "memory.V8.used"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetV8MemoryUsed, index));
        dict->SetInteger(Key(pid, "memory.sqlite"),
            InMBFromB(model_.get(),
                      &TaskManagerModel::GetSqliteMemoryUsedBytes, index));
        dict->SetString(Key(pid, "uptime"),
            ProcessUptime(model_->GetProcess(index)));
      } else {
        // TODO(cpu): Probably best to write the MD5 hash of the title.
        tabs->AppendString(model_->GetResourceTitle(index));
      }
    }
  }

  std::string ProcessUptime(base::ProcessHandle process) {
#if defined(OS_WIN)
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;

    if (!GetProcessTimes(process, &creation_time, &exit_time,
                         &kernel_time, &user_time))
      return std::string("~");

    auto ct_delta = base::Time::Now() - base::Time::FromFileTime(creation_time);
    return base::StringPrintf("%lld", ct_delta.InSeconds());
#else
    return std::string();
#endif
  }

  scoped_refptr<TaskManagerModel> model_;
  base::File file_;
};

}  // namespace

namespace caps {

void GenerateStateJSON(
    scoped_refptr<TaskManagerModel> model, base::File file) {
  new TaskManagerDataDumper(model, std::move(file));
}

}
