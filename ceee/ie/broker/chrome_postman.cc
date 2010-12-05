// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ChromePostman implementation.
#include "ceee/ie/broker/chrome_postman.h"

#include "base/string_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/common/api_registration.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/common/url_constants.h"

namespace ext = extension_automation_constants;

namespace {
  // The maximum number of times we should try to start Frame.
  const unsigned int kMaxFrameResetCount = 5;
}

class ChromeFrameMessageTask : public Task {
 public:
  explicit ChromeFrameMessageTask(
      ChromePostman::ChromePostmanThread* thread_object,
      BSTR message, BSTR target)
      : thread_object_(thread_object),
        message_(message),
        target_(target) {
  }

  virtual void Run() {
    thread_object_->PostMessage(message_, target_);
  }
 private:
  ChromePostman::ChromePostmanThread* thread_object_;
  CComBSTR message_;
  CComBSTR target_;
};


class ChromeFrameResetTask : public Task {
 public:
  explicit ChromeFrameResetTask(
      ChromePostman::ChromePostmanThread* thread_object)
      : thread_object_(thread_object) {
  }

  virtual void Run() {
    thread_object_->ResetChromeFrame();
  }
 private:
  ChromePostman::ChromePostmanThread* thread_object_;
};


class ApiExecutionTask : public Task {
 public:
  explicit ApiExecutionTask(BSTR message) : message_(message) {}

  virtual void Run() {
    ProductionApiDispatcher::get()->HandleApiRequest(message_, NULL);
  }
 private:
  CComBSTR message_;
};

class FireEventTask : public Task {
 public:
  FireEventTask(const char* event_name, const char* event_args)
      : event_name_(event_name), event_args_(event_args) {}

  virtual void Run() {
    ProductionApiDispatcher::get()->FireEvent(event_name_.c_str(),
                                              event_args_.c_str());
  }
 private:
  std::string event_name_;
  std::string event_args_;
};


ChromePostman* ChromePostman::single_instance_ = NULL;
ChromePostman::ChromePostman() {
  DCHECK(single_instance_ == NULL);
  single_instance_ = this;
  frame_reset_count_ = 0;
}

ChromePostman::~ChromePostman() {
  DCHECK(single_instance_ == this);
  single_instance_ = NULL;
}

void ChromePostman::Init() {
  // The postman thread must be a UI thread so that it can pump windows
  // messages and allow COM to handle cross apartment calls.

  chrome_postman_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_UI, 0));
  api_worker_thread_.Start();
}

void ChromePostman::Term() {
  api_worker_thread_.Stop();
  chrome_postman_thread_.Stop();
}

void ChromePostman::PostMessage(BSTR message, BSTR target) {
  MessageLoop* message_loop = chrome_postman_thread_.message_loop();
  if (message_loop) {
    // TODO(siggi@chromium.org): Remove the task subclass and change this to
    // use NewRunnableMethod.
    message_loop->PostTask(
        FROM_HERE, new ChromeFrameMessageTask(&chrome_postman_thread_,
                                              message, target));
  } else {
    LOG(ERROR) << "Trying to post a message before the postman thread is"
                  "completely initialized and ready.";
  }
}

void ChromePostman::QueueApiInvocationThreadTask(Task* task) {
  MessageLoop* message_loop = api_worker_thread_.message_loop();
  if (message_loop) {
    message_loop->PostTask(FROM_HERE, task);
  } else {
    LOG(ERROR) << "Trying to queue a task before the API worker thread is"
                  "completely initialized and ready.";
  }
}

void ChromePostman::FireEvent(const char* event_name, const char* event_args) {
  MessageLoop* message_loop = api_worker_thread_.message_loop();
  if (message_loop) {
    // TODO(siggi@chromium.org): Remove the task subclass and change this to
    // use NewRunnableMethod.
    message_loop->PostTask(FROM_HERE,
                           new FireEventTask(event_name, event_args));
  } else {
    LOG(ERROR) << "Trying to post a message before the API worker thread is"
                  "completely initialized and ready.";
  }
}

HRESULT ChromePostman::OnCfReadyStateChanged(LONG state) {
  if (state == READYSTATE_COMPLETE) {
    // If the page is fully loaded, we reset the count to 0 to ensure we restart
    // it if it's the user's fault (no max count).
    DLOG(INFO) << "frame_reset_count_ reset";
    frame_reset_count_ = 0;
  }
  return S_OK;
}

HRESULT ChromePostman::OnCfPrivateMessage(BSTR msg, BSTR origin, BSTR target) {
  if (CComBSTR(target) == ext::kAutomationRequestTarget) {
    MessageLoop* message_loop = api_worker_thread_.message_loop();
    if (message_loop) {
      message_loop->PostTask(FROM_HERE, new ApiExecutionTask(msg));
    } else {
      LOG(ERROR) << "Trying to post a message before the API worker thread is"
                    "completely initialized and ready.";
    }
  }
  return S_OK;
}

HRESULT ChromePostman::OnCfExtensionReady(BSTR path, int response) {
  return S_OK;
}

HRESULT ChromePostman::OnCfGetEnabledExtensionsComplete(
    SAFEARRAY* tab_delimited_paths) {
  return S_OK;
}

HRESULT ChromePostman::OnCfGetExtensionApisToAutomate(BSTR* functions_enabled) {
  DCHECK(functions_enabled != NULL);
  std::vector<std::string> function_names;
#define REGISTER_API_FUNCTION(func) \
  function_names.push_back(func##Function::function_name())
  REGISTER_ALL_API_FUNCTIONS();
#undef REGISTER_API_FUNCTION
  // There is a special case with CreateWindow that is explained in the
  // class comments.
  function_names.push_back(CreateWindowFunction::function_name());
  std::string function_names_delim = JoinString(function_names, ',');
  *functions_enabled = CComBSTR(function_names_delim.c_str()).Detach();

  // CF is asking for the list of extension APIs to automate so the
  // automation channel is ready.  Set a dummy URL so we get the OnLoad
  // callback.
  //
  // The current function call should come to us on the COM thread so we can
  // just call the ChromeFrameHost directly from here.
  CComPtr<IChromeFrameHost> host;
  chrome_postman_thread_.GetHost(&host);
  return host->SetUrl(CComBSTR(chrome::kAboutBlankURL));

  return S_OK;
}

HRESULT ChromePostman::OnCfChannelError() {
  MessageLoop* message_loop = chrome_postman_thread_.message_loop();
  if (message_loop) {
    frame_reset_count_++;
    LOG(INFO) << "frame_reset_count_ = " << frame_reset_count_;

    // No use staying alive if Chrome Frame can't start.
    CHECK(frame_reset_count_ < kMaxFrameResetCount)
      << "Trying to reset Chrome Frame too many times already. Something's "
         "wrong.";

    message_loop->PostTask(FROM_HERE,
                           new ChromeFrameResetTask(&chrome_postman_thread_));
  } else {
    LOG(ERROR) << "Trying to reset Chrome Frame before the postman thread is"
                  "completely initialized and ready.";
  }
  return S_OK;
}

ChromePostman::ChromePostmanThread::ChromePostmanThread()
    : base::Thread("ChromePostman") {
}

void ChromePostman::ChromePostmanThread::Init() {
  HRESULT hr = ::CoInitializeEx(0, COINIT_APARTMENTTHREADED);
  DCHECK(SUCCEEDED(hr)) << "Can't Init COM??? " << com::LogHr(hr);

  hr = InitializeChromeFrameHost();
  DCHECK(SUCCEEDED(hr)) << "InitializeChromeFrameHost failed " <<
      com::LogHr(hr);
}

HRESULT ChromePostman::ChromePostmanThread::InitializeChromeFrameHost() {
  DCHECK(thread_id() == ::GetCurrentThreadId());
  HRESULT hr = CreateChromeFrameHost();
  DCHECK(SUCCEEDED(hr) && chrome_frame_host_ != NULL);
  if (FAILED(hr) || chrome_frame_host_ == NULL) {
    LOG(ERROR) << "Failed to get chrome frame host " << com::LogHr(hr);
    return com::AlwaysError(hr);
  }

  DCHECK(ChromePostman::GetInstance() != NULL);
  chrome_frame_host_->SetEventSink(ChromePostman::GetInstance());
  chrome_frame_host_->SetChromeProfileName(
      ceee_module_util::GetBrokerProfileNameForIe());
  hr = chrome_frame_host_->StartChromeFrame();
  DCHECK(SUCCEEDED(hr));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start chrome frame " << com::LogHr(hr);
    return hr;
  }
  // We need to pin the Chrome Frame DLL so that it doesn't get unloaded
  // while we call CoUninitialize in our thread cleanup, otherwise we would
  // get stuck on breakpad termination as filed in http://crbug.com/64388.
  static bool s_pinned = false;
  if (!s_pinned) {
    HMODULE unused;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN, L"npchrome_frame.dll",
                           &unused)) {
      NOTREACHED() << "Failed to pin Chrome Frame. " << com::LogWe();
    } else {
      s_pinned = true;
    }
  }
  return hr;
}

HRESULT ChromePostman::ChromePostmanThread::CreateChromeFrameHost() {
  DCHECK(thread_id() == ::GetCurrentThreadId());
  return ChromeFrameHost::CreateInitializedIID(IID_IChromeFrameHost,
                                               &chrome_frame_host_);
}

void ChromePostman::ChromePostmanThread::CleanUp() {
  TeardownChromeFrameHost();
  ::CoUninitialize();
}

void ChromePostman::ChromePostmanThread::TeardownChromeFrameHost() {
  DCHECK(thread_id() == ::GetCurrentThreadId());
  if (chrome_frame_host_) {
    chrome_frame_host_->SetEventSink(NULL);
    HRESULT hr = chrome_frame_host_->TearDown();
    DCHECK(SUCCEEDED(hr)) << "ChromeFrameHost TearDown failed " <<
        com::LogHr(hr);
    chrome_frame_host_.Release();
  }
}

void ChromePostman::ChromePostmanThread::ResetChromeFrame() {
  TeardownChromeFrameHost();
  HRESULT hr = InitializeChromeFrameHost();
  DCHECK(SUCCEEDED(hr)) << "InitializeChromeFrameHost failed " <<
      com::LogHr(hr);
}

void ChromePostman::ChromePostmanThread::PostMessage(BSTR message,
                                                     BSTR target) {
  DCHECK(thread_id() == ::GetCurrentThreadId());
  HRESULT hr = chrome_frame_host_->PostMessage(message, target);
  DCHECK(SUCCEEDED(hr)) << "ChromeFrameHost PostMessage failed " <<
      com::LogHr(hr);
}

void ChromePostman::ChromePostmanThread::GetHost(IChromeFrameHost** host) {
  DCHECK(thread_id() == ::GetCurrentThreadId());
  chrome_frame_host_.CopyTo(host);
}

ChromePostman::ApiInvocationWorkerThread::ApiInvocationWorkerThread()
    : base::Thread("ApiInvocationWorker") {
}

void ChromePostman::ApiInvocationWorkerThread::Init() {
  ::CoInitializeEx(0, COINIT_MULTITHREADED);
  ProductionApiDispatcher::get()->SetApiInvocationThreadId(
      ::GetCurrentThreadId());
}

void ChromePostman::ApiInvocationWorkerThread::CleanUp() {
  ::CoUninitialize();
  ProductionApiDispatcher::get()->SetApiInvocationThreadId(0);
}
