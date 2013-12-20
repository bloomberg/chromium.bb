// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/MockConstraints.h"

#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

using namespace blink;

namespace WebTestRunner {

namespace {

bool isSupported(const WebString& constraint)
{
    return constraint == "valid_and_supported_1" || constraint == "valid_and_supported_2";
}

bool isValid(const WebString& constraint)
{
    return isSupported(constraint) || constraint == "valid_but_unsupported_1" || constraint == "valid_but_unsupported_2";
}

}

bool MockConstraints::verifyConstraints(const WebMediaConstraints& constraints, WebString* failedConstraint)
{
    WebVector<WebMediaConstraint> mandatoryConstraints;
    constraints.getMandatoryConstraints(mandatoryConstraints);
    if (mandatoryConstraints.size()) {
        for (size_t i = 0; i < mandatoryConstraints.size(); ++i) {
            const WebMediaConstraint& curr = mandatoryConstraints[i];
            if (!isSupported(curr.m_name) || curr.m_value != "1") {
                if (failedConstraint)
                    *failedConstraint = curr.m_name;
                return false;
            }
        }
    }

    WebVector<WebMediaConstraint> optionalConstraints;
    constraints.getOptionalConstraints(optionalConstraints);
    if (optionalConstraints.size()) {
        for (size_t i = 0; i < optionalConstraints.size(); ++i) {
            const WebMediaConstraint& curr = optionalConstraints[i];
            if (!isValid(curr.m_name) || curr.m_value != "0") {
                if (failedConstraint)
                    *failedConstraint = curr.m_name;
                return false;
            }
        }
    }

    return true;
}

}
