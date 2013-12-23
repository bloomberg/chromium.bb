// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/json_asynchronous_unpacker.h"

#include "base/command_line.h"
#include "chrome/browser/web_resource/web_resource_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

using content::BrowserThread;
using content::UtilityProcessHost;
using content::UtilityProcessHostClient;

// This class coordinates a web resource unpack and parse task which is run in
// a separate process.  Results are sent back to this class and routed to
// the WebResourceService.
class JSONAsynchronousUnpackerImpl
    : public UtilityProcessHostClient,
      public JSONAsynchronousUnpacker {
 public:
  explicit JSONAsynchronousUnpackerImpl(
      JSONAsynchronousUnpackerDelegate* delegate)
    : JSONAsynchronousUnpacker(delegate),
      got_response_(false) {
  }

  virtual void Start(const std::string& json_data) OVERRIDE {
    AddRef();  // balanced in Cleanup.

    BrowserThread::ID thread_id;
    CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &JSONAsynchronousUnpackerImpl::StartProcessOnIOThread,
            this, thread_id, json_data));
  }

 private:
  virtual ~JSONAsynchronousUnpackerImpl() {}

  // UtilityProcessHostClient.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(JSONAsynchronousUnpackerImpl, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackWebResource_Succeeded,
                          OnUnpackWebResourceSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackWebResource_Failed,
                          OnUnpackWebResourceFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  virtual void OnProcessCrashed(int exit_code) OVERRIDE {
    if (got_response_)
      return;

    OnUnpackWebResourceFailed(
        "Utility process crashed while trying to retrieve web resources.");
  }

  void OnUnpackWebResourceSucceeded(
      const base::DictionaryValue& parsed_json) {
    if (delegate_)
      delegate_->OnUnpackFinished(parsed_json);
    Cleanup();
  }

  void OnUnpackWebResourceFailed(const std::string& error_message) {
    if (delegate_)
      delegate_->OnUnpackError(error_message);
    Cleanup();
  }

  // Release reference and set got_response_.
  void Cleanup() {
    DCHECK(!got_response_);
    got_response_ = true;
    Release();
  }

  void StartProcessOnIOThread(BrowserThread::ID thread_id,
                              const std::string& json_data) {
    UtilityProcessHost* host = UtilityProcessHost::Create(
        this, BrowserThread::GetMessageLoopProxyForThread(thread_id).get());
    // TODO(mrc): get proper file path when we start using web resources
    // that need to be unpacked.
    host->Send(new ChromeUtilityMsg_UnpackWebResource(json_data));
  }

  // True if we got a response from the utility process and have cleaned up
  // already.
  bool got_response_;
};

JSONAsynchronousUnpacker* JSONAsynchronousUnpacker::Create(
    JSONAsynchronousUnpackerDelegate* delegate) {
  return new JSONAsynchronousUnpackerImpl(delegate);
}

