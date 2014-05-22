// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_constraints.h"

#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebMediaConstraint;
using blink::WebMediaConstraints;
using blink::WebString;
using blink::WebVector;

namespace content {

namespace {

bool IsSupported(const WebString& constraint) {
  return constraint == "valid_and_supported_1" ||
         constraint == "valid_and_supported_2";
}

bool IsValid(const WebString& constraint) {
  return IsSupported(constraint) || constraint == "valid_but_unsupported_1" ||
         constraint == "valid_but_unsupported_2";
}

}  // namespace

bool MockConstraints::VerifyConstraints(const WebMediaConstraints& constraints,
                                        WebString* failed_constraint) {
  WebVector<WebMediaConstraint> mandatory_constraints;
  constraints.getMandatoryConstraints(mandatory_constraints);
  if (mandatory_constraints.size()) {
    for (size_t i = 0; i < mandatory_constraints.size(); ++i) {
      const WebMediaConstraint& curr = mandatory_constraints[i];
      if (!IsSupported(curr.m_name) || curr.m_value != "1") {
        if (failed_constraint)
          *failed_constraint = curr.m_name;
        return false;
      }
    }
  }

  WebVector<WebMediaConstraint> optional_constraints;
  constraints.getOptionalConstraints(optional_constraints);
  if (optional_constraints.size()) {
    for (size_t i = 0; i < optional_constraints.size(); ++i) {
      const WebMediaConstraint& curr = optional_constraints[i];
      if (!IsValid(curr.m_name) || curr.m_value != "0") {
        if (failed_constraint)
          *failed_constraint = curr.m_name;
        return false;
      }
    }
  }

  return true;
}

}  // namespace content
