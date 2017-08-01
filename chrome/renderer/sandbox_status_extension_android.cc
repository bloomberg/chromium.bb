// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/sandbox_status_extension_android.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/seccomp_sandbox_status_android.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

SandboxStatusExtension::SandboxStatusExtension(content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

SandboxStatusExtension::~SandboxStatusExtension() {}

// static
void SandboxStatusExtension::Create(content::RenderFrame* frame) {
  auto* extension = new SandboxStatusExtension(frame);
  extension->AddRef();  // Balanced in OnDestruct().
}

void SandboxStatusExtension::OnDestruct() {
  // This object is ref-counted, since a callback could still be in-flight.
  Release();
}

bool SandboxStatusExtension::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(SandboxStatusExtension, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_AddSandboxStatusExtension,
                        OnAddSandboxStatusExtension)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SandboxStatusExtension::DidClearWindowObject() {
  Install();
}

void SandboxStatusExtension::OnAddSandboxStatusExtension() {
  should_install_ = true;
}

void SandboxStatusExtension::Install() {
  if (!should_install_)
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context->Global());
  bool success = chrome->Set(
      gin::StringToSymbol(isolate, "getAndroidSandboxStatus"),
      gin::CreateFunctionTemplate(
          isolate, base::Bind(&SandboxStatusExtension::GetSandboxStatus, this))
          ->GetFunction());
  DCHECK(success);
}

void SandboxStatusExtension::GetSandboxStatus(gin::Arguments* args) {
  if (!render_frame())
    return;

  if (render_frame()->GetWebFrame()->GetSecurityOrigin().Host() !=
      chrome::kChromeUISandboxHost) {
    args->ThrowTypeError("Not allowed on this origin");
    return;
  }

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    args->ThrowError();
    return;
  }

  auto global_callback =
      base::MakeUnique<v8::Global<v8::Function>>(args->isolate(), callback);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::Bind(&SandboxStatusExtension::ReadSandboxStatus, this),
      base::Bind(&SandboxStatusExtension::RunCallback, this,
                 base::Passed(&global_callback)));
}

std::unique_ptr<base::Value> SandboxStatusExtension::ReadSandboxStatus() {
  std::string secontext;
  base::FilePath path(FILE_PATH_LITERAL("/proc/self/attr/current"));
  base::ReadFileToString(path, &secontext);

  std::string proc_status;
  path = base::FilePath(FILE_PATH_LITERAL("/proc/self/status"));
  base::ReadFileToString(path, &proc_status);

  auto status = base::MakeUnique<base::DictionaryValue>();
  status->SetInteger("uid", getuid());
  status->SetInteger("pid", getpid());
  status->SetString("secontext", secontext);
  status->SetInteger("seccompStatus",
                     static_cast<int>(content::GetSeccompSandboxStatus()));
  status->SetString("procStatus", proc_status);
  status->SetString(
      "androidBuildId",
      base::android::BuildInfo::GetInstance()->android_build_id());

  return std::move(status);
}

void SandboxStatusExtension::RunCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback,
    std::unique_ptr<base::Value> status) {
  if (!render_frame())
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);

  v8::Local<v8::Value> argv[] = {
      content::V8ValueConverter::Create()->ToV8Value(status.get(), context)};
  render_frame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Object::New(isolate), 1, argv);
}
