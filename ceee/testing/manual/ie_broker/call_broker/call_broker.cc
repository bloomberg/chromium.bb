// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A console app driving the IE broker to and from many threads.

#include <atlbase.h>
#include <atlstr.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/automation/extension_automation_constants.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "ceee/common/com_utils.h"

#include "broker_lib.h" // NOLINT

namespace keys = extension_automation_constants;
namespace ext = extension_tabs_module_constants;

namespace {

// Unused WindowProc to create windows.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  return 1;
}

// The number of destination thread that we will create.
static const size_t kTestSize = 222;
HWND g_windows[kTestSize];
CHandle thread_ready[kTestSize];

CHandle kill_all_indexed_threads;
HMODULE g_module = NULL;

// Faking IEFrame for the Broker.
static const wchar_t* kWindowClassName = L"IEFrame";

// A ThreadProc to run all the threads that create windows and set their
// HWND in g_windows[] so that they can be used as destination threads.
DWORD WINAPI IndexedWindowCreationProc(LPVOID param) {
  size_t index = reinterpret_cast<size_t>(param);
  printf("Starting destination thread: 0x%08x, index: %d\n",
         ::GetCurrentThreadId(), index);
  g_windows[index] = ::CreateWindow(kWindowClassName, NULL, 0, 0, 0, 0, 0,
                                    NULL, NULL, NULL, 0);
  DCHECK_NE(static_cast<HWND>(NULL), g_windows[index]) <<
      com::LogWe(::GetLastError());
  // The destination thread MUST be CoInitialized.
  CoInitialize(NULL);

  // We're ready, our creator can continue.
  ::SetEvent(thread_ready[index]);
  MSG msg;
  while (true) {
    // Regular message loop for destination threads.
    DWORD result = ::MsgWaitForMultipleObjects(1, &kill_all_indexed_threads.m_h,
                                               FALSE, INFINITE, QS_POSTMESSAGE);
    if (result == WAIT_OBJECT_0) {
      // Our death signal!
      break;
    }
    DCHECK_EQ(WAIT_OBJECT_0 + 1, result);
    // We got woken up by a message, empty the queue.
    while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
      BOOL result = ::GetMessage(&msg, NULL, 0, 0);
      if (result != 0 && result != -1) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    }
  }
  return 0;
}

void CreateMessage(HWND window, int request_id, std::string* message_str) {
  // Create a JSON encoded message to call GetWindow for our window of interest.
  DictionaryValue message;
  message.SetString(keys::kAutomationNameKey, "windows.get");
  message.SetInteger(keys::kAutomationRequestIdKey, request_id);
  scoped_ptr<Value> function_args(Value::CreateIntegerValue(
      reinterpret_cast<int>(window)));
  std::string function_args_str;
  base::JSONWriter::Write(function_args.get(), false, &function_args_str);
  message.SetString(keys::kAutomationArgsKey, function_args_str);
  base::JSONWriter::Write(&message, false, message_str);
}

void ValidateResponse(BSTR response, HWND window, int request_id,
                      bool may_have_failed) {
  scoped_ptr<Value> response_value(base::JSONReader::Read(
      CW2A(response).m_psz, true));
  DCHECK(response_value.get() != NULL);
  DCHECK(response_value->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* response_dict =
      static_cast<DictionaryValue*>(response_value.get());
  DCHECK(response_dict->HasKey(keys::kAutomationRequestIdKey));
  int response_id = -1;
  response_dict->GetInteger(keys::kAutomationRequestIdKey, &response_id);
  DCHECK(response_id == request_id);
  if (response_dict->HasKey(keys::kAutomationErrorKey)) {
    // The only error we should get is if the window isn't valid anymore
    DCHECK(::IsWindow(window) == FALSE || may_have_failed);
  } else {
    DCHECK(response_dict->HasKey(keys::kAutomationResponseKey));
    std::string response_str;
    response_dict->GetString(keys::kAutomationResponseKey, &response_str);
    scoped_ptr<Value> response_value(
        base::JSONReader::Read(response_str, true));
    DCHECK(response_value.get() != NULL);
    DCHECK(response_value->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* dict = static_cast<DictionaryValue*>(response_value.get());
    DCHECK(dict->HasKey(ext::kIdKey));
    int window_id = 0;
    dict->GetInteger(ext::kIdKey, &window_id);
    DCHECK(window_id == reinterpret_cast<int>(window));
  }
}

void ExecuteAtIndex(ICeeeBroker* broker, size_t index) {
  DCHECK(index < kTestSize);
  printf("Calling Execute for Window: 0x%08x, index: %d\n",
         g_windows[index], index);
  std::string message_str;
  CreateMessage(g_windows[index], 42 + index, &message_str);
  CComBSTR response;
  HRESULT hr = broker->Execute(CComBSTR(CA2W(message_str.c_str()).m_psz),
                               &response);
  DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);
  ValidateResponse(response, g_windows[index], 42 + index, false);
}


// Source threads are created to make concurrent calls to the broker.
DWORD WINAPI IndexedSourceThreadProc(LPVOID param) {
  size_t index = reinterpret_cast<size_t>(param);
  printf("Starting source thread: 0x%08x, for index: %d\n",
         ::GetCurrentThreadId(), index);
  // Of course...
  ::CoInitialize(NULL);

  CComPtr<ICeeeBroker> broker;
  HRESULT hr = broker.CoCreateInstance(__uuidof(CeeeBroker));
  DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);

  // We aim at different destination threads to create concurrency.
  // When threads 0, 1, 2, 3 are started they all call into destination thread
  // index 0, same for 0-7 afterward, and 0-15.
  ExecuteAtIndex(broker, index / 4);
  ExecuteAtIndex(broker, index / 8);
  ExecuteAtIndex(broker, index / 16);

  printf("Done with calls from thread: 0x%08x, calling execute for index: %d\n",
         ::GetCurrentThreadId(), index);

  return 0;
}

}  // namespace


int _tmain(int argc, _TCHAR* argv[]) {
  // We need the module handle to create windows.
  BOOL success = ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
      reinterpret_cast<LPCTSTR>(WindowProc), &g_module);
  DCHECK(success) << com::LogWe(::GetLastError());

  WNDCLASS window_class;
  window_class.cbClsExtra = 0;
  window_class.cbWndExtra = 0;
  window_class.hbrBackground = NULL;
  window_class.hCursor = NULL;
  window_class.hIcon = NULL;
  window_class.hInstance = static_cast<HINSTANCE>(g_module);
  window_class.lpfnWndProc = WindowProc;
  window_class.lpszClassName = kWindowClassName;
  window_class.lpszMenuName = NULL;
  window_class.style = 0;
  ATOM window_class_atom = ::RegisterClass(&window_class);
  DCHECK_NE(static_cast<ATOM>(NULL), window_class_atom) <<
      com::LogWe(::GetLastError());

  ::CoInitialize(NULL);

  // Needed by more than one thread so manual reset.
  kill_all_indexed_threads.Attach(::CreateEvent(NULL, TRUE, FALSE, NULL));
  DCHECK_NE(static_cast<HANDLE>(NULL), kill_all_indexed_threads.m_h) <<
      com::LogWe(::GetLastError());

  // We'll also make some calls from this thread... Why not?
  CComPtr<ICeeeBroker> broker;
  HRESULT hr = broker.CoCreateInstance(__uuidof(CeeeBroker));
  DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);

  // Create all our destination threads...
  for (size_t index = 0; index < kTestSize; ++index) {
    thread_ready[index].Attach(::CreateEvent(NULL, FALSE, FALSE, NULL));
    CHandle thread_handle(::CreateThread(NULL, 0, IndexedWindowCreationProc,
        reinterpret_cast<void*>(index), 0, NULL));
    DCHECK_NE(CHandle(), thread_handle) << com::LogWe(::GetLastError());
    DWORD result = ::WaitForSingleObject(thread_ready[index], 3000);
    DCHECK_EQ(WAIT_OBJECT_0, result);
  }

  // Now create 4 times as many source threads, each making a few calls.
  CHandle last_thread_handle;
  for (size_t index = 0; index < kTestSize * 4; ++index) {
    if (index % 4 == 0) {
      CComBSTR response;
      // Once in a while, make a call from here...
      printf("Initiating call from main thread, for index: %d\n", index);
      std::string message;
      CreateMessage(g_windows[index % kTestSize], index, &message);
      hr = broker->Execute(CComBSTR(CA2W(message.c_str()).m_psz),
                           &response);
      DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);
      ValidateResponse(response, g_windows[index % kTestSize], index, false);
      printf("Done with call from main thread, for index: %d\n", index);
    } else {
      // And for the rest of the time, create a temporary caller thread.
      CHandle src_thread_handle(::CreateThread(NULL, 0, IndexedSourceThreadProc,
          reinterpret_cast<void*>(index), 0, NULL));
    }
  }

  puts("Waiting to make sure ceee_broker.exe won't go away too soon");
  ::Sleep(10000);

  printf("Initiating one last call from main thread, for index: %d\n",
         42 % kTestSize);
  std::string message;
  CreateMessage(g_windows[42 % kTestSize], 666, &message);
  CComBSTR response;
  hr = broker->Execute(CComBSTR(CA2W(message.c_str()).m_psz),
                       &response);
  DCHECK(SUCCEEDED(hr)) << com::LogHr(hr);
  ValidateResponse(response, g_windows[42 % kTestSize], 666, false);

  printf("Done with call from main thread, for index: %d\n", 42 % kTestSize);

  puts("Kill all threads.");
  ::SetEvent(kill_all_indexed_threads);
  return 0;
}
