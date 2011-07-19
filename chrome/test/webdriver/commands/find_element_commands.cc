// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/find_element_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

FindElementCommand::FindElementCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters,
    const bool find_one_element)
    : WebDriverCommand(path_segments, parameters),
      find_one_element_(find_one_element) {}

FindElementCommand::~FindElementCommand() {}

bool FindElementCommand::DoesPost() {
  return true;
}

void FindElementCommand::ExecutePost(Response* const response) {
  std::string locator, query;
  if (!GetStringParameter("using", &locator) ||
      !GetStringParameter("value", &query)) {
    response->SetError(new Error(
        kBadRequest,
        "Request is missing required 'using' and/or 'value' data"));
    return;
  }

  if (locator == "class name") {
    locator = LocatorType::kClassName;
  } else if (locator == "css selector") {
    locator = LocatorType::kCss;
  } else if (locator == "link text") {
    locator = LocatorType::kLinkText;
  } else if (locator == "partial link text") {
    locator = LocatorType::kPartialLinkText;
  } else if (locator == "tag name") {
    locator = LocatorType::kTagName;
  }
  // The other locators do not need conversion. If the client supplied an
  // invalid locator, let it fail in the atom.

  // Searching under a custom root if the URL pattern is
  // "/session/$session/element/$id/element(s)"
  WebElementId root_element(GetPathVariable(4));

  if (find_one_element_) {
    WebElementId element;
    Error* error = session_->FindElement(
        session_->current_target(), root_element, locator, query, &element);
    if (error) {
      response->SetError(error);
      return;
    }
    response->SetValue(element.ToValue());
  } else {
    std::vector<WebElementId> elements;
    Error* error = session_->FindElements(
        session_->current_target(), root_element, locator, query, &elements);
    if (error) {
      response->SetError(error);
      return;
    }
    ListValue* element_list = new ListValue();
    for (size_t i = 0; i < elements.size(); ++i)
      element_list->Append(elements[i].ToValue());
    response->SetValue(element_list);
  }
  response->SetStatus(kSuccess);
}

}  // namespace webdriver
