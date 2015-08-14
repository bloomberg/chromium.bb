// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/language_settings_private.h"

namespace extensions {

namespace language_settings_private = api::language_settings_private;

LanguageSettingsPrivateGetLanguageListFunction::
    LanguageSettingsPrivateGetLanguageListFunction() {
}

LanguageSettingsPrivateGetLanguageListFunction::
    ~LanguageSettingsPrivateGetLanguageListFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetLanguageListFunction::Run() {
  return RespondNow(OneArgument(new base::ListValue()));
}

LanguageSettingsPrivateSetLanguageListFunction::
    LanguageSettingsPrivateSetLanguageListFunction() {
}

LanguageSettingsPrivateSetLanguageListFunction::
    ~LanguageSettingsPrivateSetLanguageListFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateSetLanguageListFunction::Run() {
  scoped_ptr<language_settings_private::SetLanguageList::Params> parameters =
      language_settings_private::SetLanguageList::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() {
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::Run() {
  return RespondNow(OneArgument(new base::ListValue()));
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    LanguageSettingsPrivateGetSpellcheckWordsFunction() {
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    ~LanguageSettingsPrivateGetSpellcheckWordsFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckWordsFunction::Run() {
  return RespondNow(OneArgument(new base::ListValue()));
}

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    LanguageSettingsPrivateGetTranslateTargetLanguageFunction() {
}

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    ~LanguageSettingsPrivateGetTranslateTargetLanguageFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetTranslateTargetLanguageFunction::Run() {
  return RespondNow(OneArgument(new base::StringValue("")));
}

LanguageSettingsPrivateGetInputMethodListsFunction::
    LanguageSettingsPrivateGetInputMethodListsFunction() {
}

LanguageSettingsPrivateGetInputMethodListsFunction::
    ~LanguageSettingsPrivateGetInputMethodListsFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetInputMethodListsFunction::Run() {
  return RespondNow(OneArgument(new base::DictionaryValue()));
}

LanguageSettingsPrivateAddInputMethodFunction::
    LanguageSettingsPrivateAddInputMethodFunction() {
}

LanguageSettingsPrivateAddInputMethodFunction::
    ~LanguageSettingsPrivateAddInputMethodFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddInputMethodFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    LanguageSettingsPrivateRemoveInputMethodFunction() {
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    ~LanguageSettingsPrivateRemoveInputMethodFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveInputMethodFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

}  // namespace extensions
