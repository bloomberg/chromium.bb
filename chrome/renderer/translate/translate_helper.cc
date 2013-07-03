// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/translate/translate_util.h"
#include "chrome/renderer/translate/translate_helper_metrics.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if defined(ENABLE_LANGUAGE_DETECTION)
#include "third_party/cld/encodings/compact_lang_det/win/cld_unicodetext.h"
#endif

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeList;
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
const char kAutoDetectionLanguage[] = "auto";

// Similar language code list. Some languages are very similar and difficult
// for CLD to distinguish.
struct SimilarLanguageCode {
  const char* const code;
  int group;
};

const SimilarLanguageCode kSimilarLanguageCodes[] = {
  {"bs", 1},
  {"hr", 1},
};

// Checks |kSimilarLanguageCodes| and returns group code.
int GetSimilarLanguageGroupCode(const std::string& language) {
  for (size_t i = 0; i < arraysize(kSimilarLanguageCodes); ++i) {
    if (language.find(kSimilarLanguageCodes[i].code) != 0)
      continue;
    return kSimilarLanguageCodes[i].group;
  }
  return 0;
}

// Well-known languages which often have wrong server configuration of
// Content-Language: en.
// TODO(toyoshim): Remove these static tables and caller functions to
// chrome/common/translate, and implement them as std::set<>.
const char* kWellKnownCodesOnWrongConfiguration[] = {
  "es", "pt", "ja", "ru", "de", "zh-CN", "zh-TW", "ar", "id", "fr", "it", "th"
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      page_id_(-1),
      translation_pending_(false),
      weak_method_factory_(this) {
}

TranslateHelper::~TranslateHelper() {
  CancelPendingTranslation();
}

void TranslateHelper::PageCaptured(int page_id, const string16& contents) {
  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like langauge textbooks).  This distinction
  // shouldn't affect translation.
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame || render_view()->GetPageId() != page_id)
    return;
  page_id_ = page_id;
  WebDocument document = main_frame->document();
  std::string content_language = document.contentLanguage().utf8();
  WebElement html_element = document.documentElement();
  std::string html_lang;
  // |html_element| can be null element, e.g. in
  // BrowserTest.WindowOpenClose.
  if (!html_element.isNull())
    html_lang = html_element.getAttribute("lang").utf8();
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = DeterminePageLanguage(
      content_language, html_lang, contents, &cld_language, &is_cld_reliable);

  if (language.empty())
    return;

  language_determined_time_ = base::TimeTicks::Now();

  GURL url(document.url());
  LanguageDetectionDetails details;
  details.time = base::Time::Now();
  details.url = url;
  details.content_language = content_language;
  details.cld_language = cld_language;
  details.is_cld_reliable = is_cld_reliable;
  details.html_root_language = html_lang;
  details.adopted_language = language;

  // TODO(hajimehoshi): If this affects performance, it should be set only if
  // translate-internals tab exists.
  details.contents = contents;

  Send(new ChromeViewHostMsg_TranslateLanguageDetermined(
      routing_id(),
      details,
      IsTranslationAllowed(&document) && !language.empty()));
}

void TranslateHelper::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  translation_pending_ = false;
  source_lang_.clear();
  target_lang_.clear();
}

#if defined(ENABLE_LANGUAGE_DETECTION)
// static
std::string TranslateHelper::DetermineTextLanguage(const string16& text,
                                                   bool* is_cld_reliable) {
  std::string language = chrome::kUnknownLanguageCode;
  int num_languages = 0;
  int text_bytes = 0;
  bool is_reliable = false;
  Language cld_language =
      DetectLanguageOfUnicodeText(NULL, text.c_str(), true, &is_reliable,
                                  &num_languages, NULL, &text_bytes);
  if (is_cld_reliable != NULL)
    *is_cld_reliable = is_reliable;

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
  return ExecuteScriptAndGetBoolResult(
      "typeof cr != 'undefined' && typeof cr.googleTranslate != 'undefined' && "
      "typeof cr.googleTranslate.translate == 'function'", false);
}

bool TranslateHelper::IsTranslateLibReady() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.libReady", false);
}

bool TranslateHelper::HasTranslationFinished() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.finished", true);
}

bool TranslateHelper::HasTranslationFailed() {
  return ExecuteScriptAndGetBoolResult("cr.googleTranslate.error", true);
}

bool TranslateHelper::StartTranslation() {
  std::string script = "cr.googleTranslate.translate('" +
                       source_lang_ +
                       "','" +
                       target_lang_ +
                       "')";
  return ExecuteScriptAndGetBoolResult(script, false);
}

std::string TranslateHelper::GetOriginalPageLanguage() {
  return ExecuteScriptAndGetStringResult("cr.googleTranslate.sourceLang");
}

base::TimeDelta TranslateHelper::AdjustDelay(int delayInMs) {
  // Just converts |delayInMs| without any modification in practical cases.
  // Tests will override this function to return modified value.
  return base::TimeDelta::FromMilliseconds(delayInMs);
}

void TranslateHelper::ExecuteScript(const std::string& script) {
  WebFrame* main_frame = GetMainFrame();
  if (main_frame)
    main_frame->executeScript(WebScriptSource(ASCIIToUTF16(script)));
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool fallback) {
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsBoolean()) {
    NOTREACHED();
    return fallback;
  }

  return v->BooleanValue();
}

std::string TranslateHelper::ExecuteScriptAndGetStringResult(
    const std::string& script) {
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return std::string();

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsString()) {
    NOTREACHED();
    return std::string();
  }

  v8::Local<v8::String> v8_str = v->ToString();
  int length = v8_str->Utf8Length() + 1;
  scoped_ptr<char[]> str(new char[length]);
  v8_str->WriteUtf8(str.get(), length);
  return std::string(str.get());
}

double TranslateHelper::ExecuteScriptAndGetDoubleResult(
    const std::string& script) {
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return 0.0;

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsNumber()) {
    NOTREACHED();
    return 0.0;
  }

  return v->NumberValue();
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
bool TranslateHelper::IsValidLanguageCode(const std::string& code) {
  // Roughly check if the language code follows /[a-zA-Z]{2,3}(-[a-zA-Z]{2})?/.
  // TODO(hajimehoshi): How about es-419, which is used as an Accept language?
  std::vector<std::string> chunks;
  base::SplitString(code, '-', &chunks);

  if (chunks.size() < 1 || 2 < chunks.size())
    return false;

  const std::string& main_code = chunks[0];

  if (main_code.size() < 1 || 3 < main_code.size())
    return false;

  for (std::string::const_iterator it = main_code.begin();
       it != main_code.end(); ++it) {
    if (!IsAsciiAlpha(*it))
      return false;
  }

  if (chunks.size() == 1)
    return true;

  const std::string& sub_code = chunks[1];

  if (sub_code.size() != 2)
    return false;

  for (std::string::const_iterator it = sub_code.begin();
       it != sub_code.end(); ++it) {
    if (!IsAsciiAlpha(*it))
      return false;
  }

  return true;
}

// static
void TranslateHelper::ApplyLanguageCodeCorrection(std::string* code) {
  // Correct well-known format errors.
  CorrectLanguageCodeTypo(code);

  if (!IsValidLanguageCode(*code)) {
    *code = std::string();
    return;
  }

  TranslateUtil::ToTranslateLanguageSynonym(code);
}

// static
bool TranslateHelper::IsSameOrSimilarLanguages(
    const std::string& page_language, const std::string& cld_language) {
  // Language code part of |page_language| is matched to one of |cld_language|.
  // Country code is ignored here.
  if (page_language.size() >= 2 &&
      cld_language.find(page_language.c_str(), 0, 2) == 0) {
    // Languages are matched strictly. Reports false to metrics, but returns
    // true.
    TranslateHelperMetrics::ReportSimilarLanguageMatch(false);
    return true;
  }

  // Check if |page_language| and |cld_language| are in the similar language
  // list and belong to the same language group.
  int page_code = GetSimilarLanguageGroupCode(page_language);
  bool match = page_code != 0 &&
               page_code == GetSimilarLanguageGroupCode(cld_language);

  TranslateHelperMetrics::ReportSimilarLanguageMatch(match);
  return match;
}

// static
bool TranslateHelper::MaybeServerWrongConfiguration(
    const std::string& page_language, const std::string& cld_language) {
  // If |page_language| is not "en-*", respect it and just return false here.
  if (!StartsWithASCII(page_language, "en", false))
    return false;

  // A server provides a language meta information representing "en-*". But it
  // might be just a default value due to missing user configuration.
  // Let's trust |cld_language| if the determined language is not difficult to
  // distinguish from English, and the language is one of well-known languages
  // which often provide "en-*" meta information mistakenly.
  for (size_t i = 0; i < arraysize(kWellKnownCodesOnWrongConfiguration); ++i) {
    if (cld_language == kWellKnownCodesOnWrongConfiguration[i])
      return true;
  }
  return false;
}

// static
bool TranslateHelper::CanCLDComplementSubCode(
    const std::string& page_language, const std::string& cld_language) {
  // Translate server cannot treat general Chinese. If Content-Language and
  // CLD agree that the language is Chinese and Content-Language doesn't know
  // which dialect is used, CLD language has priority.
  // TODO(hajimehoshi): How about the other dialects like zh-MO?
  return page_language == "zh" && StartsWithASCII(cld_language, "zh-", false);
}

// static
std::string TranslateHelper::DeterminePageLanguage(const std::string& code,
                                                   const std::string& html_lang,
                                                   const string16& contents,
                                                   std::string* cld_language_p,
                                                   bool* is_cld_reliable_p) {
#if defined(ENABLE_LANGUAGE_DETECTION)
  base::TimeTicks begin_time = base::TimeTicks::Now();
  bool is_cld_reliable;
  std::string cld_language = DetermineTextLanguage(contents, &is_cld_reliable);
  TranslateHelperMetrics::ReportLanguageDetectionTime(begin_time,
                                                      base::TimeTicks::Now());

  if (cld_language_p != NULL)
    *cld_language_p = cld_language;
  if (is_cld_reliable_p != NULL)
    *is_cld_reliable_p = is_cld_reliable;
  TranslateUtil::ToTranslateLanguageSynonym(&cld_language);
#endif  // defined(ENABLE_LANGUAGE_DETECTION)

  // Check if html lang attribute is valid.
  std::string modified_html_lang;
  if (!html_lang.empty()) {
    modified_html_lang = html_lang;
    ApplyLanguageCodeCorrection(&modified_html_lang);
    TranslateHelperMetrics::ReportHtmlLang(html_lang, modified_html_lang);
    VLOG(9) << "html lang based language code: " << modified_html_lang;
  }

  // Check if Content-Language is valid.
  std::string modified_code;
  if (!code.empty()) {
    modified_code = code;
    ApplyLanguageCodeCorrection(&modified_code);
    TranslateHelperMetrics::ReportContentLanguage(code, modified_code);
  }

  // Adopt |modified_html_lang| if it is valid. Otherwise, adopt
  // |modified_code|.
  std::string language = modified_html_lang.empty() ? modified_code :
                                                      modified_html_lang;

#if defined(ENABLE_LANGUAGE_DETECTION)
  // If |language| is empty, just use CLD result even though it might be
  // chrome::kUnknownLanguageCode.
  if (language.empty()) {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_ONLY);
    return cld_language;
  }

  if (cld_language == chrome::kUnknownLanguageCode) {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_UNKNOWN);
    return language;
  } else if (IsSameOrSimilarLanguages(language, cld_language)) {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_AGREE);
    return language;
  } else if (MaybeServerWrongConfiguration(language, cld_language)) {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_TRUST_CLD);
    return cld_language;
  } else if (CanCLDComplementSubCode(language, cld_language)) {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_COMPLEMENT_SUB_CODE);
    return cld_language;
  } else {
    TranslateHelperMetrics::ReportLanguageVerification(
        TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISAGREE);
    // Content-Language value might be wrong because CLD says that this page
    // is written in another language with confidence.
    // In this case, Chrome doesn't rely on any of the language codes, and
    // gives up suggesting a translation.
    return std::string(chrome::kUnknownLanguageCode);
  }
#else  // defined(ENABLE_LANGUAGE_DETECTION)
  TranslateHelperMetrics::ReportLanguageVerification(
      TranslateHelperMetrics::LANGUAGE_VERIFICATION_CLD_DISABLED);
#endif  // defined(ENABLE_LANGUAGE_DETECTION)

  return language;
}

// static
bool TranslateHelper::IsTranslationAllowed(WebDocument* document) {
  WebElement head = document->head();
  if (head.isNull() || !head.hasChildNodes())
    return true;

  const WebString meta(ASCIIToUTF16("meta"));
  const WebString name(ASCIIToUTF16("name"));
  const WebString google(ASCIIToUTF16("google"));
  const WebString value(ASCIIToUTF16("value"));
  const WebString content(ASCIIToUTF16("content"));

  WebNodeList children = head.childNodes();
  for (size_t i = 0; i < children.length(); ++i) {
    WebNode node = children.item(i);
    if (!node.isElementNode())
      continue;
    WebElement element = node.to<WebElement>();
    // Check if a tag is <meta>.
    if (!element.hasTagName(meta))
      continue;
    // Check if the tag contains name="google".
    WebString attribute = element.getAttribute(name);
    if (attribute.isNull() || attribute != google)
      continue;
    // Check if the tag contains value="notranslate", or content="notranslate".
    attribute = element.getAttribute(value);
    if (attribute.isNull())
      attribute = element.getAttribute(content);
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
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame ||
      page_id_ != page_id ||
      render_view()->GetPageId() != page_id)
    return;  // We navigated away, nothing to do.

  // A similar translation is already under way, nothing to do.
  if (translation_pending_ && target_lang_ == target_lang)
    return;

  // Any pending translation is now irrelevant.
  CancelPendingTranslation();

  // Set our states.
  translation_pending_ = true;

  // If the source language is undetermined, we'll let the translate element
  // detect it.
  source_lang_ = (source_lang != chrome::kUnknownLanguageCode) ?
                  source_lang : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  TranslateHelperMetrics::ReportUserActionDuration(language_determined_time_,
                                                   base::TimeTicks::Now());

  GURL url(main_frame->document().url());
  TranslateHelperMetrics::ReportPageScheme(url.scheme());

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslatePageImpl(0);
}

void TranslateHelper::OnRevertTranslation(int page_id) {
  if (page_id_ != page_id || render_view()->GetPageId() != page_id)
    return;  // We navigated away, nothing to do.

  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
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

    // Check JavaScript performance counters for UMA reports.
    TranslateHelperMetrics::ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));

    // Notify the browser we are done.
    render_view()->Send(new ChromeViewHostMsg_PageTranslated(
        render_view()->GetRoutingID(), render_view()->GetPageId(),
        actual_source_lang, target_lang_, TranslateErrors::NONE));
    return;
  }

  // The translation is still pending, check again later.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
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
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TranslateHelper::TranslatePageImpl,
                   weak_method_factory_.GetWeakPtr(),
                   count),
        AdjustDelay(count * kTranslateInitCheckDelayMs));
    return;
  }

  // The library is loaded, and ready for translation now.
  // Check JavaScript performance counters for UMA reports.
  TranslateHelperMetrics::ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  TranslateHelperMetrics::ReportTimeToLoad(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.loadTime"));

  if (!StartTranslation()) {
    NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_ERROR);
    return;
  }
  // Check the status of the translation.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
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

  // When the tab is going to be closed, the web_view can be NULL.
  if (!web_view)
    return NULL;

  return web_view->mainFrame();
}
