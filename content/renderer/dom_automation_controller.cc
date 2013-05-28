// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_automation_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/string_util.h"
#include "content/common/child_process_messages.h"
#include "content/common/view_messages.h"

using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

namespace content {

DomAutomationController::DomAutomationController()
    : sender_(NULL),
      routing_id_(MSG_ROUTING_NONE),
      automation_id_(MSG_ROUTING_NONE) {
  BindCallback("send", base::Bind(&DomAutomationController::Send,
                                  base::Unretained(this)));
  BindCallback("setAutomationId",
               base::Bind(&DomAutomationController::SetAutomationId,
                          base::Unretained(this)));
  BindCallback("sendJSON", base::Bind(&DomAutomationController::SendJSON,
                                      base::Unretained(this)));
  BindCallback("sendWithId", base::Bind(&DomAutomationController::SendWithId,
                                        base::Unretained(this)));
  BindCallback("getHistogram",
               base::Bind(&DomAutomationController::GetHistogram,
                          base::Unretained(this)));
  BindCallback("getBrowserHistogram",
               base::Bind(&DomAutomationController::GetBrowserHistogram,
                          base::Unretained(this)));
}

void DomAutomationController::Send(const CppArgumentList& args,
                                   CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  if (automation_id_ == MSG_ROUTING_NONE) {
    result->SetNull();
    return;
  }

  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  std::string json;
  JSONStringValueSerializer serializer(&json);
  scoped_ptr<base::Value> value;

  // Warning: note that JSON officially requires the root-level object to be
  // an object (e.g. {foo:3}) or an array, while here we're serializing
  // strings, bools, etc. to "JSON".  This only works because (a) the JSON
  // writer is lenient, and (b) on the receiving side we wrap the JSON string
  // in square brackets, converting it to an array, then parsing it and
  // grabbing the 0th element to get the value out.
  switch (args[0].type) {
    case NPVariantType_String: {
      value.reset(new base::StringValue(args[0].ToString()));
      break;
    }
    case NPVariantType_Bool: {
      value.reset(new base::FundamentalValue(args[0].ToBoolean()));
      break;
    }
    case NPVariantType_Int32: {
      value.reset(new base::FundamentalValue(args[0].ToInt32()));
      break;
    }
    case NPVariantType_Double: {
      // The value that is sent back is an integer while it is treated
      // as a double in this binding. The reason being that KJS treats
      // any number value as a double. Refer for more details,
      // chrome/third_party/webkit/src/JavaScriptCore/bindings/c/c_utility.cpp
      value.reset(new base::FundamentalValue(args[0].ToInt32()));
      break;
    }
    default: {
      result->SetNull();
      return;
    }
  }

  if (!serializer.Serialize(*value)) {
    result->SetNull();
    return;
  }

  bool succeeded = sender_->Send(
      new ViewHostMsg_DomOperationResponse(routing_id_, json, automation_id_));
  result->Set(succeeded);

  automation_id_ = MSG_ROUTING_NONE;
}

void DomAutomationController::SendJSON(const CppArgumentList& args,
                                       CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  if (automation_id_ == MSG_ROUTING_NONE) {
    result->SetNull();
    return;
  }

  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  if (args[0].type != NPVariantType_String) {
    result->SetNull();
    return;
  }

  std::string json = args[0].ToString();
  result->Set(sender_->Send(
      new ViewHostMsg_DomOperationResponse(routing_id_, json, automation_id_)));

  automation_id_ = MSG_ROUTING_NONE;
}

void DomAutomationController::SendWithId(const CppArgumentList& args,
                                         CppVariant* result) {
  if (args.size() != 2) {
    result->SetNull();
    return;
  }

  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  if (!args[0].isNumber() || args[1].type != NPVariantType_String) {
    result->SetNull();
    return;
  }

  result->Set(sender_->Send(
      new ViewHostMsg_DomOperationResponse(routing_id_, args[1].ToString(),
                                           args[0].ToInt32())));
}

void DomAutomationController::SetAutomationId(
    const CppArgumentList& args, CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  // The check here is for NumberType and not Int32 as
  // KJS::JSType only defines a NumberType (no Int32)
  if (!args[0].isNumber()) {
    result->SetNull();
    return;
  }

  automation_id_ = args[0].ToInt32();
  result->Set(true);
}

void DomAutomationController::GetHistogram(const CppArgumentList& args,
                                           CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(args[0].ToString());
  std::string output;
  if (!histogram) {
    output = "{}";
  } else {
    histogram->WriteJSON(&output);
  }
  result->Set(output);
}

void DomAutomationController::GetBrowserHistogram(const CppArgumentList& args,
                                                  CppVariant* result) {
  if (args.size() != 1) {
    result->SetNull();
    return;
  }

  if (!sender_) {
    NOTREACHED();
    result->SetNull();
    return;
  }

  std::string histogram_json;
  sender_->Send(new ChildProcessHostMsg_GetBrowserHistogram(
      args[0].ToString(), &histogram_json));
  result->Set(histogram_json);
}

}  // namespace content
