// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_helper.h"

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
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebScriptSource;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebVector;

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

// Returns whether the page associated with |document| is a candidate for
// translation.  Some pages can explictly specify (via a meta-tag) that they
// should not be translated.
// TODO(dglazkov): This logic should be moved into Blink.
bool HasNoTranslateMeta(WebDocument* document) {
  WebElement head = document->head();
  if (head.isNull() || !head.hasChildNodes())
    return false;

  const WebString meta(ASCIIToUTF16("meta"));
  const WebString name(ASCIIToUTF16("name"));
  const WebString google(ASCIIToUTF16("google"));
  const WebString value(ASCIIToUTF16("value"));
  const WebString content(ASCIIToUTF16("content"));

  for (WebNode child = head.firstChild(); !child.isNull();
      child = child.nextSibling()) {
    if (!child.isElementNode())
      continue;
    WebElement element = child.to<WebElement>();
    // Check if a tag is <meta>.
    if (!element.hasHTMLTagName(meta))
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
    if (base::LowerCaseEqualsASCII(base::StringPiece16(attribute),
                                   "notranslate"))
      return true;
  }
  return false;
}

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(content::RenderFrame* render_frame,
                                 int world_id,
                                 int extension_group,
                                 const std::string& extension_scheme)
    : content::RenderFrameObserver(render_frame),
      page_seq_no_(0),
      translation_pending_(false),
      world_id_(world_id),
      extension_group_(extension_group),
      extension_scheme_(extension_scheme),
      weak_method_factory_(this) {}

TranslateHelper::~TranslateHelper() {
}

void TranslateHelper::PrepareForUrl(const GURL& url) {
  ++page_seq_no_;
  Send(new ChromeFrameHostMsg_TranslateAssignedSequenceNumber(routing_id(),
                                                              page_seq_no_));
}

void TranslateHelper::PageCaptured(const base::string16& contents) {
  PageCapturedImpl(page_seq_no_, contents);
}

void TranslateHelper::PageCapturedImpl(int page_seq_no,
                                       const base::string16& contents) {
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
  if (!main_frame || page_seq_no_ != page_seq_no)
    return;

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
  details.has_notranslate = HasNoTranslateMeta(&document);
  details.html_root_language = html_lang;
  details.adopted_language = language;

  // TODO(hajimehoshi): If this affects performance, it should be set only if
  // translate-internals tab exists.
  details.contents = contents;

  Send(new ChromeFrameHostMsg_TranslateLanguageDetermined(
      routing_id(), details, !details.has_notranslate && !language.empty()));
}

void TranslateHelper::CancelPendingTranslation() {
  weak_method_factory_.InvalidateWeakPtrs();
  translation_pending_ = false;
  source_lang_.clear();
  target_lang_.clear();
}

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
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      world_id_, &source, 1, extension_group_);
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool fallback) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      world_id_, &source, 1, extension_group_, &results);
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
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      world_id_, &source, 1, extension_group_, &results);
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
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      world_id_, &source, 1, extension_group_, &results);
  if (results.size() != 1 || results[0].IsEmpty() || !results[0]->IsNumber()) {
    NOTREACHED();
    return 0.0;
  }

  return results[0]->NumberValue();
}

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, private:
//
bool TranslateHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TranslateHelper, message)
    IPC_MESSAGE_HANDLER(ChromeFrameMsg_TranslatePage, OnTranslatePage)
    IPC_MESSAGE_HANDLER(ChromeFrameMsg_RevertTranslation, OnRevertTranslation)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TranslateHelper::OnTranslatePage(int page_seq_no,
                                      const std::string& translate_script,
                                      const std::string& source_lang,
                                      const std::string& target_lang) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame || page_seq_no_ != page_seq_no)
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
  source_lang_ = (source_lang != kUnknownLanguageCode) ? source_lang
                                                       : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  ReportUserActionDuration(language_determined_time_, base::TimeTicks::Now());

  GURL url(main_frame->document().url());
  ReportPageScheme(url.scheme());

  // Set up v8 isolated world with proper content-security-policy and
  // security-origin.
  main_frame->setIsolatedWorldContentSecurityPolicy(
      world_id_, WebString::fromUTF8(kContentSecurityPolicy));

  GURL security_origin = GetTranslateSecurityOrigin();
  main_frame->setIsolatedWorldSecurityOrigin(
      world_id_, WebSecurityOrigin::create(security_origin));

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  TranslatePageImpl(page_seq_no, 0);
}

void TranslateHelper::OnRevertTranslation(int page_seq_no) {
  if (page_seq_no_ != page_seq_no)
    return;  // We navigated away, nothing to do.

  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    return;
  }

  CancelPendingTranslation();

  ExecuteScript("cr.googleTranslate.revert()");
}

void TranslateHelper::CheckTranslateStatus(int page_seq_no) {
  // If this is not the same page, the translation has been canceled.
  if (page_seq_no_ != page_seq_no)
    return;

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

    if (!translation_pending_) {
      NOTREACHED();
      return;
    }

    translation_pending_ = false;

    // Check JavaScript performance counters for UMA reports.
    ReportTimeToTranslate(
        ExecuteScriptAndGetDoubleResult("cr.googleTranslate.translationTime"));

    // Notify the browser we are done.
    render_frame()->Send(new ChromeFrameHostMsg_PageTranslated(
        render_frame()->GetRoutingID(), actual_source_lang, target_lang_,
        TranslateErrors::NONE));
    return;
  }

  // The translation is still pending, check again later.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TranslateHelper::CheckTranslateStatus,
                            weak_method_factory_.GetWeakPtr(), page_seq_no),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::TranslatePageImpl(int page_seq_no, int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  if (page_seq_no_ != page_seq_no)
    return;

  if (!IsTranslateLibReady()) {
    // The library is not ready, try again later, unless we have tried several
    // times unsuccessfully already.
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(TranslateErrors::INITIALIZATION_ERROR);
      return;
    }
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TranslateHelper::TranslatePageImpl,
                   weak_method_factory_.GetWeakPtr(), page_seq_no, count),
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
                            weak_method_factory_.GetWeakPtr(), page_seq_no),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::NotifyBrowserTranslationFailed(
    TranslateErrors::Type error) {
  translation_pending_ = false;
  // Notify the browser there was an error.
  render_frame()->Send(new ChromeFrameHostMsg_PageTranslated(
      render_frame()->GetRoutingID(), source_lang_, target_lang_, error));
}

void TranslateHelper::OnDestruct() {
  delete this;
}

}  // namespace translate
