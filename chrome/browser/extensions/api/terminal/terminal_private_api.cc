// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/process_proxy/process_proxy_registry.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char* GetCroshPath() {
  if (chromeos::system::runtime_environment::IsRunningOnChromeOS())
    return kCroshCommand;
  else
    return kStubbedCroshCommand;
}

const char* GetProcessCommandForName(const std::string& name) {
  if (name == kCroshName) {
    return GetCroshPath();
  } else {
    return NULL;
  }
}

void NotifyProcessOutput(Profile* profile,
                         pid_t pid,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyProcessOutput, profile, pid, output_type, output));
    return;
  }

  base::ListValue args;
  args.Append(new base::FundamentalValue(pid));
  args.Append(new base::StringValue(output_type));
  args.Append(new base::StringValue(output));

  std::string args_json;
  base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);

  // TODO(tbarzic): Send event only to the renderer that runs process pid.
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        "terminalPrivate.onProcessOutput", args_json, NULL, GURL());
  }
}

}  // namespace

OpenTerminalProcessFunction::OpenTerminalProcessFunction() : command_(NULL) {
}

OpenTerminalProcessFunction::~OpenTerminalProcessFunction() {
}

bool OpenTerminalProcessFunction::RunImpl() {
  if (args_->GetSize() != 1)
    return false;

  std::string name;
  if (!args_->GetString(0, &name))
    return false;

  command_ = GetProcessCommandForName(name);
  if (!command_) {
    error_ = "Invalid process name.";
    return false;
  }

  // Registry lives on FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&OpenTerminalProcessFunction::OpenOnFileThread, this));
  return true;
}

void OpenTerminalProcessFunction::OpenOnFileThread() {
  DCHECK(command_);

  ProcessProxyRegistry* registry = ProcessProxyRegistry::Get();
  pid_t pid;
  if (!registry->OpenProcess(command_, &pid,
                             base::Bind(&NotifyProcessOutput, profile_))) {
    // If new process could not be opened, we return -1.
    pid = -1;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&OpenTerminalProcessFunction::RespondOnUIThread, this, pid));
}

void OpenTerminalProcessFunction::RespondOnUIThread(pid_t pid) {
  result_.reset(new base::FundamentalValue(pid));
  SendResponse(true);
}

bool SendInputToTerminalProcessFunction::RunImpl() {
  if (args_->GetSize() != 2)
    return false;

  pid_t pid;
  std::string text;
  if (!args_->GetInteger(0, &pid) || !args_->GetString(1, &text))
    return false;

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&SendInputToTerminalProcessFunction::SendInputOnFileThread,
                 this, pid, text));
  return true;
}

void SendInputToTerminalProcessFunction::SendInputOnFileThread(pid_t pid,
    const std::string& text) {
  bool success = ProcessProxyRegistry::Get()->SendInput(pid, text);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&SendInputToTerminalProcessFunction::RespondOnUIThread, this,
      success));
}

void SendInputToTerminalProcessFunction::RespondOnUIThread(bool success) {
  result_.reset(new base::FundamentalValue(success));
  SendResponse(true);
}

bool CloseTerminalProcessFunction::RunImpl() {
  if (args_->GetSize() != 1)
    return false;
  pid_t pid;
  if (!args_->GetInteger(0, &pid))
    return false;

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CloseTerminalProcessFunction::CloseOnFileThread, this, pid));

  return true;
}

void CloseTerminalProcessFunction::CloseOnFileThread(pid_t pid) {
  bool success = ProcessProxyRegistry::Get()->CloseProcess(pid);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&CloseTerminalProcessFunction::RespondOnUIThread, this,
      success));
}

void CloseTerminalProcessFunction::RespondOnUIThread(bool success) {
  result_.reset(new base::FundamentalValue(success));
  SendResponse(true);
}
