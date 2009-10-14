// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_toolstrip_api.h"

#include "base/json_writer.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/profile.h"

namespace extension_toolstrip_api_events {
const char kOnToolstripExpanded[] = "toolstrip.onExpanded.%d";
const char kOnToolstripCollapsed[] = "toolstrip.onCollapsed.%d";
};  // namespace extension_toolstrip_api_events

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

namespace keys = extension_tabs_module_constants;
namespace events = extension_toolstrip_api_events;

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

  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int height;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kHeightKey,
                                               &height));
  EXTENSION_FUNCTION_VALIDATE(height >= 0);
  if (height < kMinHeight || height > kMaxHeight) {
    error_ = kBadHeightError;
    return false;
  }

  GURL url;
  if (args->HasKey(keys::kUrlKey)) {
    std::string url_string;
    EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                &url_string));
    url = dispatcher()->url().Resolve(url_string);
    if (!url.is_valid()) {
      error_ = kInvalidURLError;
      return false;
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
    EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

    if (args->HasKey(keys::kUrlKey)) {
      std::string url_string;
      EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kUrlKey,
                                                  &url_string));
      url = dispatcher()->url().Resolve(url_string);
      if (!url.is_valid()) {
        error_ = kInvalidURLError;
        return false;
      }
    }
  }

  model_->CollapseToolstrip(toolstrip_, url);
  return true;
}

// static
void ToolstripEventRouter::DispatchEvent(Profile *profile,
                                         int routing_id,
                                         const char *event_name,
                                         const Value& json) {
  if (profile->GetExtensionMessageService()) {
    std::string json_args;
    JSONWriter::Write(&json, false, &json_args);
    std::string full_event_name = StringPrintf(event_name, routing_id);
    profile->GetExtensionMessageService()->
        DispatchEventToRenderers(full_event_name, json_args);
  }
}

// static
void ToolstripEventRouter::OnToolstripExpanded(Profile* profile,
                                               int routing_id,
                                               const GURL &url,
                                               int height) {
  ListValue args;
  DictionaryValue* obj = new DictionaryValue();
  if (!url.is_empty())
    obj->SetString(keys::kUrlKey, url.spec());
  obj->SetInteger(keys::kHeightKey, height);
  args.Append(obj);
  DispatchEvent(profile, routing_id, events::kOnToolstripExpanded, args);
}

// static
void ToolstripEventRouter::OnToolstripCollapsed(Profile* profile,
                                                int routing_id,
                                                const GURL &url) {
  ListValue args;
  DictionaryValue* obj = new DictionaryValue();
  if (!url.is_empty())
    obj->SetString(keys::kUrlKey, url.spec());
  args.Append(obj);
  DispatchEvent(profile, routing_id, events::kOnToolstripCollapsed, args);
}
