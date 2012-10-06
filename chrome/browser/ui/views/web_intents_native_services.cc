// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/string16.h"
#include "chrome/browser/intents/intent_service_host.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/web_intent_service_data.h"

namespace web_intents {
namespace {
// FilePicker service allowing a native file picker to handle
// pick+*/* intents.
class ViewsFilePickerService : public IntentServiceHost {
 public:
  ViewsFilePickerService();
  virtual void HandleIntent(content::WebIntentsDispatcher* dispatcher) OVERRIDE;

 private:
  virtual ~ViewsFilePickerService();

  DISALLOW_COPY_AND_ASSIGN(ViewsFilePickerService);
};

}  // namespace

ViewsFilePickerService::ViewsFilePickerService() {}

void ViewsFilePickerService::HandleIntent(
    content::WebIntentsDispatcher* dispatcher) {
  dispatcher->SendReplyMessage(
      webkit_glue::WEB_INTENT_REPLY_FAILURE, string16());
  delete this;
}

ViewsFilePickerService::~ViewsFilePickerService() {}

// static
IntentServiceHost* FilePickerFactory::CreateServiceInstance(
    const webkit_glue::WebIntentData& intent) {
  return new ViewsFilePickerService();
}

// Returns the action-specific string for |action|.
string16 FilePickerFactory::GetServiceTitle() {
  return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_FILE_PICKER_SERVICE_TITLE);
}

}  // web_intents namespace
