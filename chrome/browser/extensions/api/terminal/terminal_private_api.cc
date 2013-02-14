// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/process_proxy/process_proxy_registry.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char* GetCroshPath() {
  if (base::chromeos::IsRunningOnChromeOS())
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
                         const std::string& extension_id,
                         pid_t pid,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&NotifyProcessOutput, profile, extension_id,
                                         pid, output_type, output));
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::FundamentalValue(pid));
  args->Append(new base::StringValue(output_type));
  args->Append(new base::StringValue(output));

  if (profile &&
      extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        extensions::event_names::kOnTerminalProcessOutput, args.Pass()));
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToExtension(extension_id, event.Pass());
  }
}

}  // namespace

TerminalPrivateFunction::TerminalPrivateFunction() {}

TerminalPrivateFunction::~TerminalPrivateFunction() {}

bool TerminalPrivateFunction::RunImpl() {
  return RunTerminalFunction();
}

TerminalPrivateOpenTerminalProcessFunction::
    TerminalPrivateOpenTerminalProcessFunction() : command_(NULL) {}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() {}

bool TerminalPrivateOpenTerminalProcessFunction::RunTerminalFunction() {
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
      base::Bind(&TerminalPrivateOpenTerminalProcessFunction::OpenOnFileThread,
                 this));
  return true;
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnFileThread() {
  DCHECK(command_);

  ProcessProxyRegistry* registry = ProcessProxyRegistry::Get();
  pid_t pid;
  if (!registry->OpenProcess(command_, &pid,
          base::Bind(&NotifyProcessOutput, profile_, extension_id()))) {
    // If new process could not be opened, we return -1.
    pid = -1;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
                 this, pid));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() {}

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(pid_t pid) {
  SetResult(new base::FundamentalValue(pid));
  SendResponse(true);
}

bool TerminalPrivateSendInputFunction::RunTerminalFunction() {
  if (args_->GetSize() != 2)
    return false;

  pid_t pid;
  std::string text;
  if (!args_->GetInteger(0, &pid) || !args_->GetString(1, &text))
    return false;

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateSendInputFunction::SendInputOnFileThread,
                 this, pid, text));
  return true;
}

void TerminalPrivateSendInputFunction::SendInputOnFileThread(pid_t pid,
    const std::string& text) {
  bool success = ProcessProxyRegistry::Get()->SendInput(pid, text);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
      success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() {}

bool TerminalPrivateCloseTerminalProcessFunction::RunTerminalFunction() {
  if (args_->GetSize() != 1)
    return false;
  pid_t pid;
  if (!args_->GetInteger(0, &pid))
    return false;

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateCloseTerminalProcessFunction::
                 CloseOnFileThread, this, pid));

  return true;
}

void TerminalPrivateCloseTerminalProcessFunction::CloseOnFileThread(pid_t pid) {
  bool success = ProcessProxyRegistry::Get()->CloseProcess(pid);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateCloseTerminalProcessFunction::
                 RespondOnUIThread, this, success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() {}

bool TerminalPrivateOnTerminalResizeFunction::RunTerminalFunction() {
  if (args_->GetSize() != 3)
    return false;

  pid_t pid;
  if (!args_->GetInteger(0, &pid))
    return false;

  int width;
  if (!args_->GetInteger(1, &width))
    return false;

  int height;
  if (!args_->GetInteger(2, &height))
    return false;

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread,
                 this, pid, width, height));

  return true;
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread(pid_t pid,
                                                    int width, int height) {
  bool success = ProcessProxyRegistry::Get()->OnTerminalResize(pid,
                                                               width, height);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread,
                 this, success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}
