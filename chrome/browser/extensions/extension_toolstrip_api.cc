// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolstrip_api.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_shelf_model.h"

namespace extension_toolstrip_api_functions {
const char kExpandFunction[] = "toolstrip.expand";
const char kCollapseFunction[] = "toolstrip.collapse";
};  // namespace extension_toolstrip_api_functions

namespace {
// Errors.
const char kNotAToolstripError[] = "This page is not a toolstrip.";
const char kAlreadyExpandedError[] = "This toolstrip is already expanded.";
const char kAlreadyCollapsedError[] = "This toolstrip is already collapsed.";
const char kInvalidURLError[] = "Invalid URL";
const char kBadHeightError[] = "Bad height.";

// TODO(erikkay) what are good values here?
const int kMinHeight = 50;
const int kMaxHeight = 1000;
};  // namespace

bool ToolstripFunction::RunImpl() {
  ExtensionHost* host = dispatcher()->GetExtensionHost();
  if (!host) {
    error_ = kNotAToolstripError;
    return false;
  }
  Browser* browser = dispatcher()->GetBrowser();
  if (!browser) {
    error_ = kNotAToolstripError;
    return false;
  }
  model_ = browser->extension_shelf_model();
  if (!model_) {
    error_ = kNotAToolstripError;
    return false;
  }
  toolstrip_ = model_->ToolstripForHost(host);
  if (toolstrip_ == model_->end()) {
    error_ = kNotAToolstripError;
    return false;
  }
  return true;
}

bool ToolstripExpandFunction::RunImpl() {
  if (!ToolstripFunction::RunImpl())
    return false;
  if (toolstrip_->height != 0) {
    error_ = kAlreadyExpandedError;
    return false;
  }

  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);
  EXTENSION_FUNCTION_VALIDATE(args->GetSize() <= 2);

  int height;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(0, &height));
  EXTENSION_FUNCTION_VALIDATE(height >= 0);
  if (height < kMinHeight || height > kMaxHeight) {
    error_ = kBadHeightError;
    return false;
  }

  GURL url;
  if (args->GetSize() == 2) {
    Value* url_val;
    EXTENSION_FUNCTION_VALIDATE(args->Get(1, &url_val));
    if (url_val->GetType() != Value::TYPE_NULL) {
      std::string url_str;
      EXTENSION_FUNCTION_VALIDATE(url_val->GetAsString(&url_str));
      url = GURL(url_str);
      if (!url.is_valid() && !url.is_empty()) {
        error_ = kInvalidURLError;
        return false;
      }
    }
  }

  model_->ExpandToolstrip(toolstrip_, url, height);
  return true;
}

bool ToolstripCollapseFunction::RunImpl() {
  if (!ToolstripFunction::RunImpl())
    return false;

  if (toolstrip_->height == 0) {
    error_ = kAlreadyCollapsedError;
    return false;
  }

  GURL url;
  if (args_->GetType() != Value::TYPE_NULL) {
    std::string url_str;
    EXTENSION_FUNCTION_VALIDATE(args_->GetAsString(&url_str));
    url = GURL(url_str);
    if (!url.is_valid() && !url.is_empty()) {
      error_ = kInvalidURLError;
      return false;
    }
  }

  model_->CollapseToolstrip(toolstrip_, url);
  return true;
}
