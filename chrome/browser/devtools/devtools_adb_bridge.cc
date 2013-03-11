// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

static const char* kDevToolsAdbBridgeThreadName = "Chrome_DevToolsADBThread";
const int kAdbPort = 5037;

}  // namespace

// static
DevToolsAdbBridge* DevToolsAdbBridge::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new DevToolsAdbBridge();
}

void DevToolsAdbBridge::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!thread_.get())
    return;
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsAdbBridge::StopHandlerOnFileThread, this),
      base::Bind(&DevToolsAdbBridge::ResetHandlerAndReleaseOnUIThread, this));
}

void DevToolsAdbBridge::Query(
    const std::string query,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // There is a race condition in case Query immediately follows start. We
  // consider it Ok since query is polling anyways.
  if (!thread_.get()) {
    callback.Run("ADB is not yet connected", std::string());
    return;
  }
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::QueryOnHandlerThread,
                 this, query, callback));
}

DevToolsAdbBridge::DevToolsAdbBridge() {
  AddRef();  // Balanced in Release

  thread_.reset(new base::Thread(kDevToolsAdbBridgeThreadName));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsAdbBridge::StartHandlerOnFileThread, this));
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Stop() must be called prior to destruction.
  DCHECK(thread_.get() == NULL);
}

void DevToolsAdbBridge::StartHandlerOnFileThread() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DevToolsAdbBridge::ResetHandlerOnUIThread, this));
    return;
  }
}

// Runs on FILE thread to make sure that it is serialized against
// {Start|Stop}HandlerThread and to allow calling pthread_join.
void DevToolsAdbBridge::StopHandlerOnFileThread() {
  if (!thread_->message_loop())
    return;
  // Thread::Stop joins the thread.
  thread_->Stop();
}

void DevToolsAdbBridge::ResetHandlerAndReleaseOnUIThread() {
  ResetHandlerOnUIThread();
  Release();
}

void DevToolsAdbBridge::ResetHandlerOnUIThread() {
  thread_.reset();
}

void DevToolsAdbBridge::QueryOnHandlerThread(
    const std::string query,
    const Callback& callback) {
  ADBClientSocket::Query(
      kAdbPort, query,
      base::Bind(&DevToolsAdbBridge::QueryResponseOnHandlerThread,
                 base::Unretained(this), callback));
}

void DevToolsAdbBridge::QueryResponseOnHandlerThread(
    const Callback& callback,
    const std::string& error,
    const std::string& response) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DevToolsAdbBridge::RespondOnUIThread, base::Unretained(this),
          callback, error, response));
}

void DevToolsAdbBridge::RespondOnUIThread(
    const Callback& callback,
    const std::string& error,
    const std::string& response) {
  callback.Run(error, response);
}
