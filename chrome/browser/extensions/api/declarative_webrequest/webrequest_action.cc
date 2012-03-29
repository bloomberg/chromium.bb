// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/webrequest_action.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_webrequest/request_stages.h"

namespace extensions {

namespace {
// Constants from the JavaScript API.
const char kInstanceType[] = "instanceType";
const char kInstanceCancel[] = "experimental.webRequest.CancelRequest";

// Error messages.
const char kExpectedDictionary[] = "Expected a dictionary as action.";
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
}  // namespace

//
// WebRequestAction
//

WebRequestAction::WebRequestAction() {}

WebRequestAction::~WebRequestAction() {}

// static
scoped_ptr<WebRequestAction> WebRequestAction::Create(
    const base::Value& json_action,
    std::string* error) {
  const base::DictionaryValue* action_dict = NULL;
  if (!json_action.GetAsDictionary(&action_dict)) {
    *error = kExpectedDictionary;
    return scoped_ptr<WebRequestAction>(NULL);
  }

  std::string instance_type = "No instanceType";
  if (!action_dict->GetString(kInstanceType, &instance_type)) {
    *error = base::StringPrintf(kInvalidInstanceTypeError,
                                instance_type.c_str());
    return scoped_ptr<WebRequestAction>(NULL);
  }

  if (instance_type == kInstanceCancel) {
    *error = "";
    return scoped_ptr<WebRequestAction>(new WebRequestCancelAction);
  }

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return scoped_ptr<WebRequestAction>();
}


//
// WebRequestActionSet
//

WebRequestActionSet::WebRequestActionSet(const Actions& actions)
    : actions_(actions) {}

WebRequestActionSet::~WebRequestActionSet() {}

// static
scoped_ptr<WebRequestActionSet> WebRequestActionSet::Create(
    const AnyVector& actions,
    std::string* error) {
  Actions result;

  for (AnyVector::const_iterator i = actions.begin();
       i != actions.end(); ++i) {
    CHECK(i->get());
    scoped_ptr<WebRequestAction> action =
        WebRequestAction::Create((*i)->value(), error);
    if (!error->empty())
      return scoped_ptr<WebRequestActionSet>(NULL);
    result.push_back(make_linked_ptr(action.release()));
  }

  return scoped_ptr<WebRequestActionSet>(new WebRequestActionSet(result));
}


//
// WebRequestCancelAction
//

WebRequestCancelAction::WebRequestCancelAction() {}

WebRequestCancelAction::~WebRequestCancelAction() {}

int WebRequestCancelAction::GetStages() const {
  return ON_BEFORE_REQUEST | ON_BEFORE_SEND_HEADERS | ON_HEADERS_RECEIVED |
      ON_AUTH_REQUIRED;
}

WebRequestAction::Type WebRequestCancelAction::GetType() const {
  return WebRequestAction::ACTION_CANCEL_REQUEST;
}

}  // namespace extensions
