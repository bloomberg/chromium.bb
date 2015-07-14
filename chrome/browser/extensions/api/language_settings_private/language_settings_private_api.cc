// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include "base/values.h"

namespace extensions {

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
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

LanguageSettingsPrivateGetSpellCheckDictionaryStatusFunction::
    LanguageSettingsPrivateGetSpellCheckDictionaryStatusFunction() {
}

LanguageSettingsPrivateGetSpellCheckDictionaryStatusFunction::
    ~LanguageSettingsPrivateGetSpellCheckDictionaryStatusFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellCheckDictionaryStatusFunction::Run() {
  return RespondNow(OneArgument(new base::DictionaryValue()));
}

LanguageSettingsPrivateGetSpellCheckWordsFunction::
    LanguageSettingsPrivateGetSpellCheckWordsFunction() {
}

LanguageSettingsPrivateGetSpellCheckWordsFunction::
    ~LanguageSettingsPrivateGetSpellCheckWordsFunction() {
}

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellCheckWordsFunction::Run() {
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
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
