// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/dictionary_event_router.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace chromeos {

ExtensionDictionaryEventRouter::ExtensionDictionaryEventRouter(
    content::BrowserContext* context)
    : context_(context), loaded_() {
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (spellcheck) {
    service_ = spellcheck->GetWeakPtr();
    service_->GetCustomDictionary()->AddObserver(this);
    loaded_ = service_->GetCustomDictionary()->IsLoaded();
  }
}

ExtensionDictionaryEventRouter::~ExtensionDictionaryEventRouter() {
  if (service_)
    service_->GetCustomDictionary()->RemoveObserver(this);
}

void ExtensionDictionaryEventRouter::DispatchLoadedEventIfLoaded() {
  if (!loaded_)
    return;

  extensions::EventRouter* router = extensions::EventRouter::Get(context_);
  if (!router->HasEventListener(
      extensions::InputMethodAPI::kOnDictionaryLoaded)) {
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue());
  // The router will only send the event to extensions that are listening.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::InputMethodAPI::kOnDictionaryLoaded, args.Pass()));
  event->restrict_to_browser_context = context_;
  router->BroadcastEvent(event.Pass());
}

void ExtensionDictionaryEventRouter::OnCustomDictionaryLoaded() {
  loaded_ = true;
  DispatchLoadedEventIfLoaded();
}

void ExtensionDictionaryEventRouter::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(
      extensions::InputMethodAPI::kOnDictionaryChanged)) {
    return;
  }

  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<base::ListValue> added_words(new base::ListValue());
  scoped_ptr<base::ListValue> removed_words(new base::ListValue());
  added_words->AppendStrings(dictionary_change.to_add());
  removed_words->AppendStrings(dictionary_change.to_remove());
  args->Append(added_words.release());
  args->Append(removed_words.release());

  // The router will only send the event to extensions that are listening.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::InputMethodAPI::kOnDictionaryChanged, args.Pass()));
  event->restrict_to_browser_context = context_;
  router->BroadcastEvent(event.Pass());
}

}  // namespace chromeos
