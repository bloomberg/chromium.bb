// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_api.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/search_engines_private.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateGetDefaultSearchEnginesFunction

SearchEnginesPrivateGetDefaultSearchEnginesFunction::
    SearchEnginesPrivateGetDefaultSearchEnginesFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateGetDefaultSearchEnginesFunction::
    ~SearchEnginesPrivateGetDefaultSearchEnginesFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateGetDefaultSearchEnginesFunction::Run() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  base::ListValue* engines = new base::ListValue();

  const TemplateURL* default_url =
      template_url_service->GetDefaultSearchProvider();
  std::vector<TemplateURL*> urls = template_url_service->GetTemplateURLs();
  for (size_t i = 0; i < urls.size(); i++) {
    api::search_engines_private::SearchEngine engine;
    engine.guid = urls[i]->sync_guid();
    engine.name = base::UTF16ToASCII(urls[i]->short_name());
    if (urls[i] == default_url)
      engine.is_selected = scoped_ptr<bool>(new bool(true));

    engines->Append(engine.ToValue().release());
  }

  return RespondNow(OneArgument(engines));
}

////////////////////////////////////////////////////////////////////////////////
// SearchEnginesPrivateSetSelectedSearchEngineFunction

SearchEnginesPrivateSetSelectedSearchEngineFunction::
    SearchEnginesPrivateSetSelectedSearchEngineFunction()
    : chrome_details_(this) {
}

SearchEnginesPrivateSetSelectedSearchEngineFunction::
    ~SearchEnginesPrivateSetSelectedSearchEngineFunction() {
}

ExtensionFunction::ResponseAction
SearchEnginesPrivateSetSelectedSearchEngineFunction::Run() {
  scoped_ptr<api::search_engines_private::SetSelectedSearchEngine::Params>
      parameters =
          api::search_engines_private::SetSelectedSearchEngine::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(chrome_details_.GetProfile());
  template_url_service->SetUserSelectedDefaultSearchProvider(
      template_url_service->GetTemplateURLForGUID(parameters->guid));
  return RespondNow(NoArguments());
}

}  // namespace extensions
