// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/js_app.h"

#include "base/bind.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "mojo/apps/js/application_delegate_impl.h"
#include "mojo/apps/js/mojo_module.h"
#include "mojo/common/data_pipe_utils.h"

namespace mojo {
namespace apps {

JSApp::JSApp(ApplicationDelegateImpl* content_handler_app,
             const std::string& url,
             URLResponsePtr content)
    : content_handler_app_(content_handler_app),
      url_(url),
      content_(content.Pass()),
      thread_("Mojo JS " + url),
      content_handler_task_runner_(
          base::MessageLoop::current()->task_runner()) {
  CHECK(on_content_handler_thread());
  runner_delegate_.AddBuiltinModule(Mojo::kModuleName,
                                    base::Bind(Mojo::GetModule, this));
}

JSApp::~JSApp() {
}

bool JSApp::Start() {
  CHECK(!js_app_task_runner_.get() && on_content_handler_thread());
  base::Thread::Options thread_options(base::MessageLoop::TYPE_IO, 0);
  thread_.StartWithOptions(thread_options);

  // TODO(hansmuller): check thread_.StartWithOptions() return value.
  // TODO(hansmuller): need to funnel Run() failures back to the caller.

  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&JSApp::Run, base::Unretained(this)));
  return true;
}

void JSApp::Quit() {
  CHECK(on_js_app_thread());

  // The terminate operation is posted to the message_loop so that
  // the shell_runner isn't destroyed before this JS function returns.
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&JSApp::Terminate, base::Unretained(this)));
}

Handle JSApp::ConnectToService(const std::string& application_url,
                               const std::string& interface_name) {
  CHECK(on_js_app_thread());
  MessagePipe pipe;

  content_handler_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ApplicationDelegateImpl::ConnectToService,
                 base::Unretained(content_handler_app_),
                 base::Passed(pipe.handle1.Pass()),
                 application_url,
                 interface_name));

  return pipe.handle0.release();
}

void JSApp::Run() {
  CHECK(!js_app_task_runner_.get() && !on_content_handler_thread());
  js_app_task_runner_ = base::MessageLoop::current()->task_runner();

  // TODO(hansmuller): check the return value and fail gracefully.
  std::string module;
  common::BlockingCopyToString(content_->body.Pass(), &module);

  gin::IsolateHolder::Initialize(gin::IsolateHolder::kStrictMode,
                                 gin::ArrayBufferAllocator::SharedInstance());
  isolate_holder_.reset(new gin::IsolateHolder());
  isolate_holder_->AddRunMicrotasksObserver();

  shell_runner_.reset(
      new gin::ShellRunner(&runner_delegate_, isolate_holder_->isolate()));

  // TODO(hansmuller): exiting this scope here is OK?
  gin::Runner::Scope scope(shell_runner_.get());
  shell_runner_->Run(module.c_str(), url_.c_str());
}

void JSApp::Terminate() {
  isolate_holder_->RemoveRunMicrotasksObserver();
  shell_runner_.reset(NULL);

  // This JSApp's thread must be stopped on the thread that started it. Ask the
  // content_handler_app_ to erase its AppVector entry for this app, which
  // implicitly destroys this JSApp and stops its thread.
  content_handler_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ApplicationDelegateImpl::QuitJSApp,
                 base::Unretained(content_handler_app_),
                 base::Unretained(this)));
}

bool JSApp::on_content_handler_thread() const {
  return content_handler_task_runner_.get() &&
         content_handler_task_runner_.get() ==
             base::MessageLoop::current()->task_runner().get();
}

bool JSApp::on_js_app_thread() const {
  return js_app_task_runner_.get() &&
         js_app_task_runner_.get() ==
             base::MessageLoop::current()->task_runner().get();
}

}  // namespace apps
}  // namespace mojo
