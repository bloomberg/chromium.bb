// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/translate_helper.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidget.h"
#include "url/gurl.h"
#include "v8/include/v8.h"

using base::ASCIIToUTF16;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebNode;
using blink::WebNodeList;
using blink::WebScriptSource;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebVector;
using blink::WebView;

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

// Whether or not we have set the CLD callback yet.
bool g_cld_callback_set = false;

}  // namespace

namespace translate {

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(content::RenderView* render_view,
                                 int world_id,
                                 int extension_group,
                                 const std::string& extension_scheme)
    : content::RenderViewObserver(render_view),
      page_seq_no_(0),
      translation_pending_(false),
      cld_data_provider_(translate::CreateRendererCldDataProviderFor(this)),
      cld_data_polling_started_(false),
      cld_data_polling_canceled_(false),
      deferred_page_capture_(false),
      deferred_page_seq_no_(-1),
      world_id_(world_id),
      extension_group_(extension_group),
      extension_scheme_(extension_scheme),
      weak_method_factory_(this) {
}

TranslateHelper::~TranslateHelper() {
  CancelPendingTranslation();
  CancelCldDataPolling();
}

void TranslateHelper::PrepareForUrl(const GURL& url) {
  ++page_seq_no_;
  Send(new ChromeViewHostMsg_TranslateAssignedSequenceNumber(
      routing_id(), page_seq_no_));
  deferred_page_capture_ = false;
  deferred_page_seq_no_ = -1;
  deferred_contents_.clear();
  if (cld_data_polling_started_)
    return;

  // TODO(andrewhayden): Refactor translate_manager.cc's IsTranslatableURL to
  // components/translate/core/common/translate_util.cc, and ignore any URL
  // that fails that check. This will require moving unit tests and rewiring
  // other function calls as well, so for now replicate the logic here.
  if (url.is_empty())
    return;
  if (url.SchemeIs(content::kChromeUIScheme))
    return;
  if (url.SchemeIs(content::kChromeDevToolsScheme))
    return;
  if (url.SchemeIs(url::kFtpScheme))
    return;
  if (url.SchemeIs(extension_scheme_.c_str()))
    return;

  // Start polling for CLD data.
  cld_data_polling_started_ = true;
  TranslateHelper::SendCldDataRequest(0, 1000);
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
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame || page_seq_no_ != page_seq_no)
    return;

  if (!cld_data_provider_->IsCldDataAvailable()) {
    // We're in dynamic mode and CLD data isn't loaded. Retry when CLD data
    // is loaded, if ever.
    deferred_page_capture_ = true;
    deferred_page_seq_no_ = page_seq_no;
    deferred_contents_ = contents;
    RecordLanguageDetectionTiming(DEFERRED);
    return;
  }

  if (deferred_page_seq_no_ == -1) {
    // CLD data was available before language detection was requested.
    RecordLanguageDetectionTiming(ON_TIME);
  } else {
    // This is a request that was triggered because CLD data is now available
    // and was previously deferred.
    RecordLanguageDetectionTiming(RESUMED);
  }

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
  CancelCldDataPolling();
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
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      world_id_, &source, 1, extension_group_);
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool fallback) {
  WebFrame* main_frame = GetMainFrame();
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
  WebFrame* main_frame = GetMainFrame();
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

  v8::Local<v8::String> v8_str = results[0]->ToString();
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
  if (!handled) {
    handled = cld_data_provider_->OnMessageReceived(message);
  }
  return handled;
}

void TranslateHelper::OnTranslatePage(int page_seq_no,
                                      const std::string& translate_script,
                                      const std::string& source_lang,
                                      const std::string& target_lang) {
  WebFrame* main_frame = GetMainFrame();
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
  WebFrame* frame = GetMainFrame();
  if (frame) {
    frame->setIsolatedWorldContentSecurityPolicy(
        world_id_, WebString::fromUTF8(kContentSecurityPolicy));

    GURL security_origin = GetTranslateSecurityOrigin();
    frame->setIsolatedWorldSecurityOrigin(
        world_id_, WebSecurityOrigin::create(security_origin));
  }

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
  // If this is not the same page, the translation has been canceled.  If the
  // view is gone, the page is closing.
  if (page_seq_no_ != page_seq_no || !render_view()->GetWebView())
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
    render_view()->Send(
        new ChromeViewHostMsg_PageTranslated(render_view()->GetRoutingID(),
                                             actual_source_lang,
                                             target_lang_,
                                             TranslateErrors::NONE));
    return;
  }

  // The translation is still pending, check again later.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr(), page_seq_no),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::TranslatePageImpl(int page_seq_no, int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  if (page_seq_no_ != page_seq_no || !render_view()->GetWebView())
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
                   page_seq_no, count),
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
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::CheckTranslateStatus,
                 weak_method_factory_.GetWeakPtr(), page_seq_no),
      AdjustDelay(kTranslateStatusCheckDelayMs));
}

void TranslateHelper::NotifyBrowserTranslationFailed(
    TranslateErrors::Type error) {
  translation_pending_ = false;
  // Notify the browser there was an error.
  render_view()->Send(new ChromeViewHostMsg_PageTranslated(
      render_view()->GetRoutingID(), source_lang_, target_lang_, error));
}

WebFrame* TranslateHelper::GetMainFrame() {
  WebView* web_view = render_view()->GetWebView();

  // When the tab is going to be closed, the web_view can be NULL.
  if (!web_view)
    return NULL;

  return web_view->mainFrame();
}

void TranslateHelper::CancelCldDataPolling() {
  cld_data_polling_canceled_ = true;
}

void TranslateHelper::SendCldDataRequest(const int delay_millis,
                                         const int next_delay_millis) {
  // Terminate immediately if told to stop polling.
  if (cld_data_polling_canceled_)
    return;

  // Terminate immediately if data is already loaded.
  if (cld_data_provider_->IsCldDataAvailable())
    return;

  if (!g_cld_callback_set) {
    g_cld_callback_set = true;
    cld_data_provider_->SetCldAvailableCallback(
        base::Bind(&TranslateHelper::OnCldDataAvailable,
                   weak_method_factory_.GetWeakPtr()));
  }

  // Else, make an asynchronous request to get the data we need.
  cld_data_provider_->SendCldDataRequest();

  // ... and enqueue another delayed task to call again. This will start a
  // chain of polling that will last until the pointer stops being NULL,
  // which is the right thing to do.
  // NB: In the great majority of cases, the data file will be available and
  // the very first delayed task will be a no-op that terminates the chain.
  // It's only while downloading the file that this will chain for a
  // nontrivial amount of time.
  // Use a weak pointer to avoid keeping this helper object around forever.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TranslateHelper::SendCldDataRequest,
                 weak_method_factory_.GetWeakPtr(),
                 next_delay_millis,
                 next_delay_millis),
      base::TimeDelta::FromMilliseconds(delay_millis));
}

void TranslateHelper::OnCldDataAvailable() {
  if (deferred_page_capture_) {
    deferred_page_capture_ = false; // Don't do this a second time.
    PageCapturedImpl(deferred_page_seq_no_, deferred_contents_);
    deferred_page_seq_no_ = -1; // Clean up for sanity
    deferred_contents_.clear(); // Clean up for sanity
  }
}

void TranslateHelper::RecordLanguageDetectionTiming(
    LanguageDetectionTiming timing) {
  // The following comment is copied from page_load_histograms.cc, and applies
  // just as equally here:
  //
  // Since there are currently no guarantees that renderer histograms will be
  // sent to the browser, we initiate a PostTask here to be sure that we send
  // the histograms we generated.  Without this call, pages that don't have an
  // on-close-handler might generate data that is lost when the renderer is
  // shutdown abruptly (perchance because the user closed the tab).
  DVLOG(1) << "Language detection timing: " << timing;
  UMA_HISTOGRAM_ENUMERATION("Translate.LanguageDetectionTiming", timing,
                            LANGUAGE_DETECTION_TIMING_MAX_VALUE);

  // Note on performance: Under normal circumstances, this should get called
  // once per page load. The code will either manage to do it ON_TIME or will
  // be DEFERRED until CLD is ready. In the latter case, CLD is in dynamic mode
  // and may eventually become available, triggering the RESUMED event; after
  // this, everything should start being ON_TIME. This should never run more
  // than twice in a page load, under any conditions.
  // Also note that language detection is triggered off of a delay AFTER the
  // page load completed event has fired, making this very much off the critical
  // path.
  content::RenderThread::Get()->UpdateHistograms(
      content::kHistogramSynchronizerReservedSequenceNumber);
}

}  // namespace translate
