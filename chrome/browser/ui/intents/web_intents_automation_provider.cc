// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intents_automation_provider.h"

#include "base/values.h"
#include "chrome/browser/intents/cws_intents_registry.h"

WebIntentsAutomationProvider::WebIntentsAutomationProvider(
    WebIntentPickerController* controller)
    : controller_(controller) {
}

WebIntentsAutomationProvider::~WebIntentsAutomationProvider() {
}

bool WebIntentsAutomationProvider::FillSuggestedExtensionList(
    const ListValue* extensions) {
  DCHECK(extensions);
  for (ListValue::const_iterator it = extensions->begin();
      it != extensions->end(); ++it) {
    DictionaryValue* extension = NULL;
    if (!(*it)->GetAsDictionary(&extension))
      return false;

    std::string icon_url;
    CWSIntentsRegistry::IntentExtensionInfo extension_info;

    if (!extension->GetString("name", &extension_info.name) ||
        !extension->GetString("id", &extension_info.id) ||
        !extension->GetDouble("average_rating",
                              &extension_info.average_rating) ||
        !extension->GetString("icon_url", &icon_url))
      return false;
    extension_info.icon_url = GURL(icon_url);
    suggested_extensions_.push_back(extension_info);
  }
  return true;
}

void WebIntentsAutomationProvider::SetSuggestedExtensions() {
  for (size_t i = 0; i < suggested_extensions_.size(); ++i)
    controller_->AddSuggestedExtension(suggested_extensions_[i]);
  controller_->AsyncOperationFinished();
}
