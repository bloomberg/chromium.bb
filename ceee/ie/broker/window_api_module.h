// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window API implementation.

#ifndef CEEE_IE_BROKER_WINDOW_API_MODULE_H_
#define CEEE_IE_BROKER_WINDOW_API_MODULE_H_

#include <string>

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/common_api_module.h"

#include "toolband.h"  // NOLINT

namespace window_api {

class WindowApiResult;
typedef ApiResultCreator<WindowApiResult> WindowApiResultCreator;

// Registers all Window API invocations with the given dispatcher.
void RegisterInvocations(ApiDispatcher* dispatcher);

class WindowApiResult : public common_api::CommonApiResult {
 public:
  explicit WindowApiResult(int request_id)
      : common_api::CommonApiResult(request_id) {}

  // Updates the position of the given window based on the arguments given and
  // sets the result value appropriately. Calls PostError() if there is an
  // error and returns false.
  // @param window The window to update.
  // @param window_rect The arguments for the window update, a DictionaryValue
  //        containing the left, top, width, height to update the window with.
  virtual bool UpdateWindowRect(HWND window,
                                const DictionaryValue* window_rect);
};

typedef IterativeApiResult<WindowApiResult> IterativeWindowApiResult;

class GetWindow : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class GetCurrentWindow : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id,
                       const DictionaryValue* associated_tab);
};

class GetLastFocusedWindow : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

// Unfortunately winuser.h uses a #define for CreateWindow to use either
// the ASCII or Wide char version, so it replaces the Constructor declaration
// with CreateWindowW() and fails compilation if we use CreateWindow as a
// class name. So we must have another class name and use a special case to
// specify the name of the function we replace in Chrome in RegisterInvocations.
class CreateWindowFunc : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  // public so that the RegisterInvocations can see it.
  // Simply reacts to an OnWindowCreated event and waits for the window to
  // be completely created before letting the ApiDispatcher broadcast the event.
  static bool EventHandler(const std::string& input_args,
                           std::string* converted_args,
                           ApiDispatcher* dispatcher);
};

class UpdateWindow : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
};

class RemoveWindow : public WindowApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
  static bool EventHandler(const std::string& input_args,
                           std::string* converted_args,
                           ApiDispatcher* dispatcher);
};

class GetAllWindows : public ApiResultCreator<IterativeWindowApiResult> {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  static void FillResult(IterativeWindowApiResult* result, bool populate_tabs);
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
};

}  // namespace window_api

#endif  // CEEE_IE_BROKER_WINDOW_API_MODULE_H_
