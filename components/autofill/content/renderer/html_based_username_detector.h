// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "components/autofill/core/common/password_form.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

namespace autofill {

using UsernameDetectorCache =
    std::map<blink::WebFormElement, blink::WebInputElement>;

// Classifier for getting username field by analyzing HTML attribute values.
// The algorithm looks for words that are likely to point to username field
// (ex. "username", "loginid" etc.), in the attribute values. When the first
// match is found, the currently analyzed field is saved in |username_element|,
// and the algorithm ends. By searching for words in order of their probability
// to be username words, it is sure that the first match will also be the best
// one. The function returns true if username element was found.
// If the result for the given form is cached in |username_detector_cache|, then
// |username_element| is set to the cached result. Otherwise, the classifier
// will be run and the outcome will be saved to the cache.
// |username_detector_cache| can be null.
bool GetUsernameFieldBasedOnHtmlAttributes(
    const std::vector<blink::WebInputElement>& all_possible_usernames,
    const FormData& form_data,
    blink::WebInputElement* username_element,
    UsernameDetectorCache* username_detector_cache);

}  // namespace autofill
