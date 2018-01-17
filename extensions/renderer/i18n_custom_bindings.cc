// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/i18n_custom_bindings.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/message_bundle.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"

namespace extensions {

using namespace v8_helpers;

namespace {

// Max number of languages to detect.
const int kCldNumLangs = 3;

// CLD3 minimum reliable byte threshold. Predictions for inputs below this size
// in bytes will be considered unreliable.
const int kCld3MinimumByteThreshold = 50;

struct DetectedLanguage {
  DetectedLanguage(const std::string& language, int percentage)
      : language(language), percentage(percentage) {}
  ~DetectedLanguage() {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this DetectedLanguage object.
  std::unique_ptr<base::DictionaryValue> ToDictionary() const;

  std::string language;
  int percentage;

 private:
  DISALLOW_COPY_AND_ASSIGN(DetectedLanguage);
};

// LanguageDetectionResult object that holds detected langugae reliability and
// array of DetectedLanguage
struct LanguageDetectionResult {
  LanguageDetectionResult() {}
  explicit LanguageDetectionResult(bool is_reliable)
      : is_reliable(is_reliable) {}
  ~LanguageDetectionResult() {}

  // Returns a new v8::Local<v8::Value> representing the serialized form of
  // this Result object.
  v8::Local<v8::Value> ToValue(ScriptContext* context);

  // CLD detected language reliability
  bool is_reliable;

  // Array of detectedLanguage of size 1-3. The null is returned if
  // there were no languages detected
  std::vector<std::unique_ptr<DetectedLanguage>> languages;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageDetectionResult);
};

std::unique_ptr<base::DictionaryValue> DetectedLanguage::ToDictionary() const {
  std::unique_ptr<base::DictionaryValue> dict_value(
      new base::DictionaryValue());
  dict_value->SetString("language", language.c_str());
  dict_value->SetInteger("percentage", percentage);
  return dict_value;
}

v8::Local<v8::Value> LanguageDetectionResult::ToValue(ScriptContext* context) {
  base::DictionaryValue dict_value;
  dict_value.SetBoolean("isReliable", is_reliable);
  std::unique_ptr<base::ListValue> languages_list(new base::ListValue());
  for (const auto& language : languages)
    languages_list->Append(language->ToDictionary());
  dict_value.Set("languages", std::move(languages_list));

  v8::Local<v8::Context> v8_context = context->v8_context();
  v8::Isolate* isolate = v8_context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Value> result =
      content::V8ValueConverter::Create()->ToV8Value(&dict_value, v8_context);
  return handle_scope.Escape(result);
}

void InitDetectedLanguages(
    const std::vector<chrome_lang_id::NNetLanguageIdentifier::Result>&
        lang_results,
    LanguageDetectionResult* result) {
  std::vector<std::unique_ptr<DetectedLanguage>>* detected_languages =
      &result->languages;
  DCHECK(detected_languages->empty());
  bool* is_reliable = &result->is_reliable;

  // is_reliable is set to "true", so that the reliability can be calculated by
  // &&'ing the reliability of each predicted language.
  *is_reliable = true;
  for (size_t i = 0; i < lang_results.size(); ++i) {
    const chrome_lang_id::NNetLanguageIdentifier::Result& lang_result =
        lang_results.at(i);
    const std::string& language_code = lang_result.language;

    // If a language is kUnknown, then the remaining ones are also kUnknown.
    if (language_code == chrome_lang_id::NNetLanguageIdentifier::kUnknown) {
      break;
    }

    // The list of languages supported by CLD3 is saved in kLanguageNames
    // in the following file:
    // //src/third_party/cld_3/src/src/task_context_params.cc
    // Among the entries in this list are transliterated languages
    // (called xx-Latn) which don't belong to the spec ISO639-1 used by
    // the previous model, CLD2. Thus, to maintain backwards compatibility,
    // xx-Latn predictions are ignored for now.
    if (base::EndsWith(language_code, "-Latn",
                       base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    *is_reliable = *is_reliable && lang_result.is_reliable;
    const int percent = static_cast<int>(100 * lang_result.proportion);
    detected_languages->push_back(
        std::make_unique<DetectedLanguage>(language_code, percent));
  }

  if (detected_languages->empty()) {
    *is_reliable = false;
  }
}

}  // namespace

I18NCustomBindings::I18NCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetL10nMessage", "i18n",
      base::Bind(&I18NCustomBindings::GetL10nMessage, base::Unretained(this)));
  RouteFunction("GetL10nUILanguage", "i18n",
                base::Bind(&I18NCustomBindings::GetL10nUILanguage,
                           base::Unretained(this)));
  RouteFunction("DetectTextLanguage", "i18n",
                base::Bind(&I18NCustomBindings::DetectTextLanguage,
                           base::Unretained(this)));
}

void I18NCustomBindings::GetL10nMessage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 3 || !args[0]->IsString()) {
    NOTREACHED() << "Bad arguments";
    return;
  }

  v8::Isolate* isolate = args.GetIsolate();
  std::string extension_id;
  if (args[2]->IsNull() || !args[2]->IsString()) {
    return;
  } else {
    extension_id = *v8::String::Utf8Value(isolate, args[2]);
    if (extension_id.empty())
      return;
  }

  L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    content::RenderFrame* render_frame = context()->GetRenderFrame();
    if (!render_frame)
      return;

    L10nMessagesMap messages;
    // A sync call to load message catalogs for current extension.
    {
      SCOPED_UMA_HISTOGRAM_TIMER("Extensions.SyncGetMessageBundle");
      render_frame->Send(
          new ExtensionHostMsg_GetMessageBundle(extension_id, &messages));
    }

    // Save messages we got.
    ExtensionToL10nMessagesMap& l10n_messages_map =
        *GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = GetL10nMessagesMap(extension_id);
  }

  std::string message_name = *v8::String::Utf8Value(isolate, args[0]);
  std::string message =
      MessageBundle::GetL10nMessage(message_name, *l10n_messages);

  std::vector<std::string> substitutions;
  if (args[1]->IsArray()) {
    // chrome.i18n.getMessage("message_name", ["more", "params"]);
    v8::Local<v8::Array> placeholders = v8::Local<v8::Array>::Cast(args[1]);
    uint32_t count = placeholders->Length();
    if (count > 9)
      return;
    for (uint32_t i = 0; i < count; ++i) {
      substitutions.push_back(*v8::String::Utf8Value(
          isolate, placeholders->Get(v8::Integer::New(isolate, i))));
    }
  } else if (args[1]->IsString()) {
    // chrome.i18n.getMessage("message_name", "one param");
    substitutions.push_back(*v8::String::Utf8Value(isolate, args[1]));
  }

  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      isolate,
      base::ReplaceStringPlaceholders(message, substitutions, NULL).c_str()));
}

void I18NCustomBindings::GetL10nUILanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(v8::String::NewFromUtf8(
      args.GetIsolate(), content::RenderThread::Get()->GetLocale().c_str()));
}

void I18NCustomBindings::DetectTextLanguage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 1);
  CHECK(args[0]->IsString());

  std::string text = *v8::String::Utf8Value(args.GetIsolate(), args[0]);
  chrome_lang_id::NNetLanguageIdentifier nnet_lang_id(/*min_num_bytes=*/0,
                                                      /*max_num_bytes=*/512);
  std::vector<chrome_lang_id::NNetLanguageIdentifier::Result> lang_results =
      nnet_lang_id.FindTopNMostFreqLangs(text, kCldNumLangs);

  // is_reliable is set to false if we believe the input is too short to be
  // accurately identified by the current model.
  if (text.size() < kCld3MinimumByteThreshold) {
    for (auto& result : lang_results) {
      result.is_reliable = false;
    }
  }

  LanguageDetectionResult result;

  // Populate LanguageDetectionResult with prediction reliability, languages,
  // and the corresponding percentages.
  InitDetectedLanguages(lang_results, &result);
  args.GetReturnValue().Set(result.ToValue(context()));
}

}  // namespace extensions
