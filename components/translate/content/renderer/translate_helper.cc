// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLanguageDetectionDetails.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using blink::WebDocument;
using blink::WebLocalFrame;
using blink::WebScriptSource;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebVector;
using blink::WebLanguageDetectionDetails;

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

// Isolated world sets following content-security-policy.
const char kContentSecurityPolicy[] = "script-src 'self' 'unsafe-eval'";

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
TranslateHelper::TranslateHelper(content::RenderFrame* render_frame,
                                 int world_id,
                                 const std::string& extension_scheme)
    : content::RenderFrameObserver(render_frame),
      world_id_(world_id),
      extension_scheme_(extension_scheme),
      binding_(this),
      weak_method_factory_(this) {}

TranslateHelper::~TranslateHelper() {
}

void TranslateHelper::PrepareForUrl(const GURL& url) {
  // Navigated to a new url, reset current page translation.
  ResetPage();
}

void TranslateHelper::PageCaptured(const base::string16& contents) {
  // Get the document language as set by WebKit from the http-equiv
  // meta tag for "content-language".  This may or may not also
  // have a value derived from the actual Content-Language HTTP
  // header.  The two actually have different meanings (despite the
  // original intent of http-equiv to be an equivalent) with the former
  // being the language of the document and the latter being the
  // language of the intended audience (a distinction really only
  // relevant for things like langauge textbooks).  This distinction
  // shouldn't affect translation.
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebDocument document = main_frame->GetDocument();
  WebLanguageDetectionDetails web_detection_details =
      WebLanguageDetectionDetails::CollectLanguageDetectionDetails(document);
  std::string content_language = web_detection_details.content_language.Utf8();
  std::string html_lang = web_detection_details.html_language.Utf8();
  std::string cld_language;
  bool is_cld_reliable;
  std::string language = DeterminePageLanguage(
      content_language, html_lang, contents, &cld_language, &is_cld_reliable);

  if (language.empty())
    return;

  language_determined_time_ = base::TimeTicks::Now();

  LanguageDetectionDetails details;
  details.time = base::Time::Now();
  details.url = web_detection_details.url;
  details.content_language = content_language;
  details.cld_language = cld_language;
  details.is_cld_reliable = is_cld_reliable;
  details.has_notranslate = web_detection_details.has_no_translate_meta;
  details.html_root_language = html_lang;
  details.adopted_language = language;

  // TODO(hajimehoshi): If this affects performance, it should be set only if
  // translate-internals tab exists.
  details.contents = contents;

  // For the same render frame with the same url, each time when its texts are
  // captured, it should be treated as a new page to do translation.
  ResetPage();
  GetTranslateDriver()->RegisterPage(
      binding_.CreateInterfacePtrAndBind(), details,
      !details.has_notranslate && !language.empty());
}

void TranslateHelper::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  // Make sure to send the cancelled response back.
  if (translate_callback_pending_) {
    std::move(translate_callback_pending_)
        .Run(true, source_lang_, target_lang_, TranslateErrors::NONE);
  }
  source_lang_.clear();
  target_lang_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, protected:
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
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  main_frame->ExecuteScriptInIsolatedWorld(world_id_, &source, 1);
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool fallback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  main_frame->ExecuteScriptInIsolatedWorld(world_id_, &source, 1, &results);
  if (results.size() != 1 || results[0].IsEmpty() || !results[0]->IsBoolean()) {
    NOTREACHED();
    return fallback;
  }

  return results[0]->BooleanValue();
}

std::string TranslateHelper::ExecuteScriptAndGetStringResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return std::string();

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  main_frame->ExecuteScriptInIsolatedWorld(world_id_, &source, 1, &results);
  if (results.size() != 1 || results[0].IsEmpty() || !results[0]->IsString()) {
    NOTREACHED();
    return std::string();
  }

  v8::Local<v8::String> v8_str = results[0].As<v8::String>();
  int length = v8_str->Utf8Length() + 1;
  std::unique_ptr<char[]> str(new char[length]);
  v8_str->WriteUtf8(str.get(), length);
  return std::string(str.get());
}

double TranslateHelper::ExecuteScriptAndGetDoubleResult(
    const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return 0.0;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(WebString::FromASCII(script));
  main_frame->ExecuteScriptInIsolatedWorld(world_id_, &source, 1, &results);
  if (results.size() != 1 || results[0].IsEmpty() || !results[0]->IsNumber()) {
    NOTREACHED();
    return 0.0;
  }

  return results[0]->NumberValue();
}

// mojom::Page implementations.
void TranslateHelper::Translate(const std::string& translate_script,
                                const std::string& source_lang,
                                const std::string& target_lang,
                                TranslateCallback callback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame) {
    // Cancelled.
    std::move(callback).Run(true, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;  // We navigated away, nothing to do.
  }

  // A similar translation is already under way, nothing to do.
  if (translate_callback_pending_ && target_lang_ == target_lang) {
    // This request is ignored.
    std::move(callback).Run(true, source_lang, target_lang,
                            TranslateErrors::NONE);
    return;
  }

  // Any pending translation is now irrelevant.
  CancelPendingTranslation();

  // Set our states.
  translate_callback_pending_ = std::move(callback);

  // If the source language is undetermined, we'll let the translate element
  // detect it.
  source_lang_ = (source_lang != kUnknownLanguageCode) ? source_lang
                                                       : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  ReportUserActionDuration(language_determined_time_, base::TimeTicks::Now());

  GURL url(main_frame->GetDocument().Url());
  ReportPageScheme(url.scheme());

  // Set up v8 isolated world with proper content-security-policy and
  // security-origin.
  main_frame->SetIsolatedWorldContentSecurityPolicy(
      world_id_, WebString::FromUTF8(kContentSecurityPolicy));

  GURL security_origin = GetTranslateSecurityOrigin();
  main_frame->SetIsolatedWorldSecurityOrigin(
      world_id_, WebSecurityOrigin::Create(security_origin));

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslatePageImpl(0);
}

void TranslateHelper::RevertTranslation() {
  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
}

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, private:
void TranslateHelper::CheckTranslateStatus() {
  // First check if there was an error.
  if (HasTranslationFailed()) {
    // TODO(toyoshim): Check |errorCode| of translate.js and notify it here.
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

    if (!translate_callback_pending_) {
      NOTREACHED();
      return;
    }

    // Check JavaScript performance counters for UMA reports.
    ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));

    // Notify the browser we are done.
    std::move(translate_callback_pending_)
        .Run(false, actual_source_lang, target_lang_, TranslateErrors::NONE);
    return;
  }

  // The translation is still pending, check again later.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TranslateHelper::CheckTranslateStatus,
                            weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::TranslatePageImpl(int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  if (!IsTranslateLibReady()) {
    // The library is not ready, try again later, unless we have tried several
    // times unsuccessfully already.
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::INITIALIZATION_ERROR);
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&TranslateHelper::TranslatePageImpl,
                              weak_method_factory_.GetWeakPtr(), count),
        AdjustDelay(count * kTranslateInitCheckDelayMs));
    return;
  }

  // The library is loaded, and ready for translation now.
  // Check JavaScript performance counters for UMA reports.
  ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  ReportTimeToLoad(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.loadTime"));

  if (!StartTranslation()) {
    NotifyBrowserTranslationFailed(TranslateErrors::TRANSLATION_ERROR);
    return;
  }
  // Check the status of the translation.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TranslateHelper::CheckTranslateStatus,
                            weak_method_factory_.GetWeakPtr()),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::NotifyBrowserTranslationFailed(
    TranslateErrors::Type error) {
  DCHECK(translate_callback_pending_);
  // Notify the browser there was an error.
  std::move(translate_callback_pending_)
      .Run(false, source_lang_, target_lang_, error);
}

const mojom::ContentTranslateDriverPtr& TranslateHelper::GetTranslateDriver() {
  if (!translate_driver_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&translate_driver_));
  }

  return translate_driver_;
}

void TranslateHelper::ResetPage() {
  binding_.Close();
  translate_callback_pending_.Reset();
  CancelPendingTranslation();
}

void TranslateHelper::OnDestruct() {
  delete this;
}

}  // namespace translate
