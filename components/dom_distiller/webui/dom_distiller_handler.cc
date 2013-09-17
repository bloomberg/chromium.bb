// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/webui/dom_distiller_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace dom_distiller {

DomDistillerHandler::DomDistillerHandler()
    : weak_ptr_factory_(this) {
}

DomDistillerHandler::~DomDistillerHandler() {}

void DomDistillerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestEntries",
      base::Bind(&DomDistillerHandler::HandleRequestEntries,
      base::Unretained(this)));
}

void DomDistillerHandler::HandleRequestEntries(const ListValue* args) {
  base::ListValue entries;

  // Add some temporary placeholder entries.
  scoped_ptr<base::DictionaryValue> entry1(new base::DictionaryValue());
  entry1->SetString("title", "Google");
  entry1->SetString("url", "http://www.google.com/");
  entries.Append(entry1.release());
  scoped_ptr<base::DictionaryValue> entry2(new base::DictionaryValue());
  entry2->SetString("title", "Chrome");
  entry2->SetString("url", "http://www.chrome.com/");
  entries.Append(entry2.release());

  web_ui()->CallJavascriptFunction("onGotEntries", entries);
}

}  // namespace dom_distiller
