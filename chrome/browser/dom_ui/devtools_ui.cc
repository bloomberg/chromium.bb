// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/devtools_ui.h"

#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"

class DevToolsMessageHandler: public DOMMessageHandler {
 public:
  explicit DevToolsMessageHandler(ExtensionsService *extensions_service):
    extensions_service_(extensions_service) {}

 protected:
  void HandleRequestDevToolsExtensionsData(const Value* value) {
    DCHECK(!value);

    ListValue results;
    const ExtensionList* extensions = extensions_service_->extensions();

    for (ExtensionList::const_iterator extension = extensions->begin();
         extension != extensions->end(); ++extension) {
      if ((*extension)->devtools_url().is_empty())
        continue;
      DictionaryValue* extension_info = new DictionaryValue();
      extension_info->Set(L"startPage",
          new StringValue((*extension)->devtools_url().spec()));
      results.Append(extension_info);
    }
    dom_ui_->CallJavascriptFunction(L"WebInspector.addExtensions", results);
  }

  virtual void RegisterMessages() {
    dom_ui_->RegisterMessageCallback("requestDevToolsExtensionsData",
        NewCallback(this,
            &DevToolsMessageHandler::HandleRequestDevToolsExtensionsData));
  }

 private:
  scoped_refptr<ExtensionsService> extensions_service_;
};

DevToolsUI::DevToolsUI(TabContents* contents) : DOMUI(contents) {
  DevToolsMessageHandler* handler = new DevToolsMessageHandler(
      GetProfile()->GetOriginalProfile()->GetExtensionsService());
  AddMessageHandler(handler->Attach(this));
}

void DevToolsUI::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host->Send(new ViewMsg_SetupDevToolsClient(
      render_view_host->routing_id()));
}
