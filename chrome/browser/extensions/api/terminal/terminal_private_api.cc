// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace SendInput = extensions::api::terminal_private::SendInput;

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char* GetCroshPath() {
  if (base::SysInfo::IsRunningOnChromeOS())
    return kCroshCommand;
  else
    return kStubbedCroshCommand;
}

const char* GetProcessCommandForName(const std::string& name) {
  if (name == kCroshName)
    return GetCroshPath();
  else
    return NULL;
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

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (profile && event_router) {
    scoped_ptr<extensions::Event> event(new extensions::Event(
        terminal_private::OnProcessOutput::kEventName, args.Pass()));
        event_router->DispatchEventToExtension(extension_id, event.Pass());
  }
}

}  // namespace

namespace extensions {

TerminalPrivateFunction::TerminalPrivateFunction() {}

TerminalPrivateFunction::~TerminalPrivateFunction() {}

bool TerminalPrivateFunction::RunAsync() {
  return RunTerminalFunction();
}

TerminalPrivateOpenTerminalProcessFunction::
    TerminalPrivateOpenTerminalProcessFunction() : command_(NULL) {}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() {}

bool TerminalPrivateOpenTerminalProcessFunction::RunTerminalFunction() {
  scoped_ptr<OpenTerminalProcess::Params> params(
      OpenTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  command_ = GetProcessCommandForName(params->process_name);
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

  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();
  pid_t pid;
  if (!registry->OpenProcess(
           command_,
           &pid,
           base::Bind(&NotifyProcessOutput, GetProfile(), extension_id()))) {
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
  scoped_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateSendInputFunction::SendInputOnFileThread,
                 this, params->pid, params->input));
  return true;
}

void TerminalPrivateSendInputFunction::SendInputOnFileThread(pid_t pid,
    const std::string& text) {
  bool success = chromeos::ProcessProxyRegistry::Get()->SendInput(pid, text);

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
  bool success = chromeos::ProcessProxyRegistry::Get()->CloseProcess(pid);

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
  scoped_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on the FILE thread.
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread,
                 this, params->pid, params->width, params->height));

  return true;
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnFileThread(pid_t pid,
                                                    int width, int height) {
  bool success = chromeos::ProcessProxyRegistry::Get()->OnTerminalResize(
      pid, width, height);

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread,
                 this, success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
}

}  // namespace extensions
