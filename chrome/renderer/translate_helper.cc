// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"
#include "webkit/glue/dom_operations.h"

#if defined(ENABLE_LANGUAGE_DETECTION)
#include "third_party/cld/encodings/compact_lang_det/win/cld_unicodetext.h"
#endif

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebView;

namespace {

// The delay in milliseconds that we'll wait before checking to see if the
// translate library injected in the page is ready.
const int kTranslateInitCheckDelayMs = 150;

// The maximum number of times we'll check to see if the translate library
// injected in the page is ready.
const int kMaxTranslateInitCheckAttempts = 5;

// The delay we wait in milliseconds before checking whether the translation has
// finished.
const int kTranslateStatusCheckDelayMs = 400;

// Language name passed to the Translate element for it to detect the language.
const char* const kAutoDetectionLanguage = "auto";

// Language code synonyms. Some languages have changed codes over the years
// and sometimes the older codes are used, so we must see them as synonyms.
struct LanguageCodeSynonym {
  const char* const to;  // code used in supporting list
  const char* const from;  // synonym code
};

const LanguageCodeSynonym kLanguageCodeSynonyms[] = {
  {"no", "nb"},
  {"iw", "he"},
  {"jw", "jv"},
  {"tl", "fil"},
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      translation_pending_(false),
      page_id_(-1),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_method_factory_(this)) {
}

TranslateHelper::~TranslateHelper() {
  CancelPendingTranslation();
}

void TranslateHelper::PageCaptured(const string16& contents) {
  WebDocument document = render_view()->GetWebView()->mainFrame()->document();

  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like langauge textbooks).  This distinction
  // shouldn't affect translation.
  std::string content_language = document.contentLanguage().utf8();
  std::string language = DeterminePageLanguage(content_language, contents);
  if (language.empty())
    return;

  Send(new ChromeViewHostMsg_TranslateLanguageDetermined(
      routing_id(), language, IsPageTranslatable(&document)));
}

void TranslateHelper::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  translation_pending_ = false;
  page_id_ = -1;
  source_lang_.clear();
  target_lang_.clear();
}

#if defined(ENABLE_LANGUAGE_DETECTION)
// static
std::string TranslateHelper::DetermineTextLanguage(const string16& text) {
  std::string language = chrome::kUnknownLanguageCode;
  int num_languages = 0;
  int text_bytes = 0;
  bool is_reliable = false;
  Language cld_language =
      DetectLanguageOfUnicodeText(NULL, text.c_str(), true, &is_reliable,
                                  &num_languages, NULL, &text_bytes);
  // We don't trust the result if the CLD reports that the detection is not
  // reliable, or if the actual text used to detect the language was less than
  // 100 bytes (short texts can often lead to wrong results).
  // TODO(toyoshim): CLD provides |is_reliable| flag. But, it just says that
  // the determined language code is correct with 50% confidence. Chrome should
  // handle the real confidence value to judge.
  if (is_reliable && text_bytes >= 100 && cld_language != NUM_LANGUAGES &&
      cld_language != UNKNOWN_LANGUAGE && cld_language != TG_UNKNOWN_LANGUAGE) {
    // We should not use LanguageCode_ISO_639_1 because it does not cover all
    // the languages CLD can detect. As a result, it'll return the invalid
    // language code for tradtional Chinese among others.
    // |LanguageCodeWithDialect| will go through ISO 639-1, ISO-639-2 and
    // 'other' tables to do the 'right' thing. In addition, it'll return zh-CN
    // for Simplified Chinese.
    language = LanguageCodeWithDialects(cld_language);
  }
  VLOG(9) << "Detected lang_id: " << language << ", from Text:\n" << text
          << "\n*************************************\n";
  return language;
}
#endif  // defined(ENABLE_LANGUAGE_DETECTION)

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, protected:
//
bool TranslateHelper::IsTranslateLibAvailable() {
  bool lib_available = false;
  if (!ExecuteScriptAndGetBoolResult(
      "typeof cr != 'undefined' && typeof cr.googleTranslate != 'undefined' && "
      "typeof cr.googleTranslate.translate == 'function'", &lib_available)) {
    NOTREACHED();
    return false;
  }
  return lib_available;
}

bool TranslateHelper::IsTranslateLibReady() {
  bool lib_ready = false;
  if (!ExecuteScriptAndGetBoolResult("cr.googleTranslate.libReady",
                                     &lib_ready)) {
    NOTREACHED();
    return false;
  }
  return lib_ready;
}

bool TranslateHelper::HasTranslationFinished() {
  bool translation_finished = false;
  if (!ExecuteScriptAndGetBoolResult("cr.googleTranslate.finished",
                                     &translation_finished)) {
    NOTREACHED() << "crGoogleTranslateGetFinished returned unexpected value.";
    return true;
  }

  return translation_finished;
}

bool TranslateHelper::HasTranslationFailed() {
  bool translation_failed = false;
  if (!ExecuteScriptAndGetBoolResult("cr.googleTranslate.error",
                                     &translation_failed)) {
    NOTREACHED() << "crGoogleTranslateGetError returned unexpected value.";
    return true;
  }

  return translation_failed;
}

bool TranslateHelper::StartTranslation() {
  bool translate_success = false;
  if (!ExecuteScriptAndGetBoolResult("cr.googleTranslate.translate('" +
                                     source_lang_ + "','" + target_lang_ + "')",
                                     &translate_success)) {
    NOTREACHED();
    return false;
  }
  return translate_success;
}

std::string TranslateHelper::GetOriginalPageLanguage() {
  std::string lang;
  ExecuteScriptAndGetStringResult("cr.googleTranslate.sourceLang", &lang);
  return lang;
}

bool TranslateHelper::DontDelayTasks() {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, private:
//
// static
void TranslateHelper::CorrectLanguageCodeTypo(std::string* code) {
  DCHECK(code);

  size_t coma_index = code->find(',');
  if (coma_index != std::string::npos) {
    // There are more than 1 language specified, just keep the first one.
    *code = code->substr(0, coma_index);
  }
  TrimWhitespaceASCII(*code, TRIM_ALL, code);

  // An underscore instead of a dash is a frequent mistake.
  size_t underscore_index = code->find('_');
  if (underscore_index != std::string::npos)
    (*code)[underscore_index] = '-';

  // Change everything up to a dash to lower-case and everything after to upper.
  size_t dash_index = code->find('-');
  if (dash_index != std::string::npos) {
    *code = StringToLowerASCII(code->substr(0, dash_index)) +
        StringToUpperASCII(code->substr(dash_index));
  } else {
    *code = StringToLowerASCII(*code);
  }
}

// static
void TranslateHelper::ConvertLanguageCodeSynonym(std::string* code) {
  DCHECK(code);

  // Apply liner search here because number of items in the list is just four.
  for (size_t i = 0; i < arraysize(kLanguageCodeSynonyms); ++i) {
    if (code->compare(kLanguageCodeSynonyms[i].from) == 0) {
      *code = std::string(kLanguageCodeSynonyms[i].to);
      break;
    }
  }
}

// static
void TranslateHelper::ResetInvalidLanguageCode(std::string* code) {
  // Roughly check if the language code follows [a-z][a-z](-[A-Z][A-Z]).
  size_t dash_index = code->find('-');
  if (!(dash_index == 2 && code->size() == 5) &&
      !(dash_index == std::string::npos && code->size() == 2)) {
    // Reset |language| to ignore the invalid code.
    *code = std::string();
  }
}

// static
std::string TranslateHelper::DeterminePageLanguage(const std::string& code,
                                                   const string16& contents) {
#if defined(ENABLE_LANGUAGE_DETECTION)
  base::TimeTicks begin_time = base::TimeTicks::Now();
  std::string cld_language = DetermineTextLanguage(contents);
  UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.LanguageDetection",
                             base::TimeTicks::Now() - begin_time);
  ConvertLanguageCodeSynonym(&cld_language);
  VLOG(9) << "CLD determined language code: " << cld_language;

  // If |code| is empty, just use CLD result even though it might be
  // chrome::kUnknownLanguageCode.
  if (code.empty())
    return cld_language;
#endif  // defined(ENABLE_LANGUAGE_DETECTION)

  // Correct well-known format errors.
  std::string language = code;
  CorrectLanguageCodeTypo(&language);

  // Convert language code synonym firstly because sometime synonym code is in
  // invalid format, e.g. 'fil'. After validation, such a 3 characters language
  // gets converted to an empty string.
  ConvertLanguageCodeSynonym(&language);
  ResetInvalidLanguageCode(&language);
  VLOG(9) << "Content-Language based language code: " << language;

#if defined(ENABLE_LANGUAGE_DETECTION)
  if (cld_language != chrome::kUnknownLanguageCode &&
      cld_language != language) {
    // Content-Language value might be wrong because CLD says that this page
    // is written in another language with confidence.
    // In this case, Chrome doesn't rely on any of the language codes, and
    // gives up suggesting a translation.
    VLOG(9) << "CLD disagreed with the Content-Language value with confidence.";
    return std::string(chrome::kUnknownLanguageCode);
  }
#endif  // defined(ENABLE_LANGUAGE_DETECTION)

  return language;
}

// static
bool TranslateHelper::IsPageTranslatable(WebDocument* document) {
  std::vector<WebElement> meta_elements;
  webkit_glue::GetMetaElementsWithAttribute(document,
                                            ASCIIToUTF16("name"),
                                            ASCIIToUTF16("google"),
                                            &meta_elements);
  std::vector<WebElement>::const_iterator iter;
  for (iter = meta_elements.begin(); iter != meta_elements.end(); ++iter) {
    WebString attribute = iter->getAttribute("value");
    if (attribute.isNull())  // We support both 'value' and 'content'.
      attribute = iter->getAttribute("content");
    if (attribute.isNull())
      continue;
    if (LowerCaseEqualsASCII(attribute, "notranslate"))
      return false;
  }
  return true;
}

bool TranslateHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TranslateHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_TranslatePage, OnTranslatePage)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RevertTranslation, OnRevertTranslation)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TranslateHelper::OnTranslatePage(int page_id,
                                      const std::string& translate_script,
                                      const std::string& source_lang,
                                      const std::string& target_lang) {
  if (render_view()->GetPageId() != page_id)
    return;  // We navigated away, nothing to do.

  if (translation_pending_ && page_id == page_id_ &&
      target_lang_ == target_lang) {
    // A similar translation is already under way, nothing to do.
    return;
  }

  // Any pending translation is now irrelevant.
  CancelPendingTranslation();

  // Set our states.
  translation_pending_ = true;
  page_id_ = page_id;
  // If the source language is undetermined, we'll let the translate element
  // detect it.
  source_lang_ = (source_lang != chrome::kUnknownLanguageCode) ?
                  source_lang : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslatePageImpl(0);
}

void TranslateHelper::OnRevertTranslation(int page_id) {
  if (render_view()->GetPageId() != page_id)
    return;  // We navigated away, nothing to do.

  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    return;
  }

  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return;

  CancelPendingTranslation();

  main_frame->executeScript(
      WebScriptSource(ASCIIToUTF16("cr.googleTranslate.revert()")));
}

void TranslateHelper::CheckTranslateStatus() {
  // If this is not the same page, the translation has been canceled.  If the
  // view is gone, the page is closing.
  if (page_id_ != render_view()->GetPageId() || !render_view()->GetWebView())
    return;

  // First check if there was an error.
  if (HasTranslationFailed()) {
    NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_ERROR);
    return;  // There was an error.
  }

  if (HasTranslationFinished()) {
    std::string actual_source_lang;
    // Translation was successfull, if it was auto, retrieve the source
    // language the Translate Element detected.
    if (source_lang_ == kAutoDetectionLanguage) {
      actual_source_lang = GetOriginalPageLanguage();
      if (actual_source_lang.empty()) {
        NotifyBrowserTranslationFailed(TranslateErrors::UNKNOWN_LANGUAGE);
        return;
      } else if (actual_source_lang == target_lang_) {
        NotifyBrowserTranslationFailed(TranslateErrors::IDENTICAL_LANGUAGES);
        return;
      }
    } else {
      actual_source_lang = source_lang_;
    }

    if (!translation_pending_) {
      NOTREACHED();
      return;
    }

    translation_pending_ = false;

    // Notify the browser we are done.
    render_view()->Send(new ChromeViewHostMsg_PageTranslated(
        render_view()->GetRoutingID(), render_view()->GetPageId(),
        actual_source_lang, target_lang_, TranslateErrors::NONE));
    return;
  }

  // The translation is still pending, check again later.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          DontDelayTasks() ? 0 : kTranslateStatusCheckDelayMs));
}

bool TranslateHelper::ExecuteScript(const std::string& script) {
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return false;
  main_frame->executeScript(WebScriptSource(ASCIIToUTF16(script)));
  return true;
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool* value) {
  DCHECK(value);
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return false;

  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsBoolean())
    return false;

  *value = v->BooleanValue();
  return true;
}

bool TranslateHelper::ExecuteScriptAndGetStringResult(const std::string& script,
                                                      std::string* value) {
  DCHECK(value);
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return false;

  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsString())
    return false;

  v8::Local<v8::String> v8_str = v->ToString();
  int length = v8_str->Utf8Length() + 1;
  scoped_array<char> str(new char[length]);
  v8_str->WriteUtf8(str.get(), length);
  *value = str.get();
  return true;
}

void TranslateHelper::TranslatePageImpl(int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  if (page_id_ != render_view()->GetPageId() || !render_view()->GetWebView())
    return;

  if (!IsTranslateLibReady()) {
    // The library is not ready, try again later, unless we have tried several
    // times unsucessfully already.
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::INITIALIZATION_ERROR);
      return;
    }
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TranslateHelper::TranslatePageImpl,
                   weak_method_factory_.GetWeakPtr(), count),
        base::TimeDelta::FromMilliseconds(
            DontDelayTasks() ? 0 : count * kTranslateInitCheckDelayMs));
    return;
  }

  if (!StartTranslation()) {
    NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_ERROR);
    return;
  }
  // Check the status of the translation.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(
          DontDelayTasks() ? 0 : kTranslateStatusCheckDelayMs));
}

void TranslateHelper::NotifyBrowserTranslationFailed(
    TranslateErrors::Type error) {
  translation_pending_ = false;
  // Notify the browser there was an error.
  render_view()->Send(new ChromeViewHostMsg_PageTranslated(
      render_view()->GetRoutingID(), page_id_, source_lang_,
      target_lang_, error));
}

WebFrame* TranslateHelper::GetMainFrame() {
  WebView* web_view = render_view()->GetWebView();
  if (!web_view) {
    // When the WebView is going away, the render view should have called
    // CancelPendingTranslation() which should have stopped any pending work, so
    // that case should not happen.
    NOTREACHED();
    return NULL;
  }
  return web_view->mainFrame();
}
