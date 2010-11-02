// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Infobar API implementation.

#ifndef CEEE_IE_BROKER_INFOBAR_API_MODULE_H_
#define CEEE_IE_BROKER_INFOBAR_API_MODULE_H_

#include <string>

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/common_api_module.h"

namespace infobar_api {

class InfobarApiResult;
typedef ApiResultCreator<InfobarApiResult> InfobarApiResultCreator;

// Registers all Infobar API invocations with the given dispatcher.
void RegisterInvocations(ApiDispatcher* dispatcher);

class InfobarApiResult : public common_api::CommonApiResult {
 public:
  explicit InfobarApiResult(int request_id)
      : common_api::CommonApiResult(request_id) {}
};

typedef IterativeApiResult<InfobarApiResult> IterativeInfobarApiResult;

class ShowInfoBar : public InfobarApiResultCreator {
 public:
  virtual void Execute(const ListValue& args, int request_id);
  // We need to wait for the infobar browser ready event to complete the
  // result response.
  static HRESULT ContinueExecution(const std::string& input_args,
                                   ApiDispatcher::InvocationResult* user_data,
                                   ApiDispatcher* dispatcher);
};

}  // namespace infobar_api

#endif  // CEEE_IE_BROKER_INFOBAR_API_MODULE_H_
