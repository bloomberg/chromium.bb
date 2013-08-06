// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/translate/language_detection_util.h"
#include "chrome/common/translate/translate_common_metrics.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/isolated_world_ids.h"
#include "content/public/renderer/render_view.h"
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

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebNodeList;
using WebKit::WebScriptSource;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebVector;
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

// Isolated world sets following content-security-policy.
const char kContentSecurityPolicy[] = "script-src 'self' 'unsafe-eval'";

// Isolated world sets following security-origin by default.
const char kSecurityOrigin[] = "https://translate.googleapis.com";

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
  std::string language = LanguageDetectionUtil::DeterminePageLanguage(
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
      chrome::ISOLATED_WORLD_ID_TRANSLATE,
      &source,
      1,
      extensions::EXTENSION_GROUP_INTERNAL_TRANSLATE_SCRIPTS);
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool fallback) {
  WebFrame* main_frame = GetMainFrame();
  if (!main_frame)
    return fallback;

  v8::HandleScope handle_scope;
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      chrome::ISOLATED_WORLD_ID_TRANSLATE,
      &source,
      1,
      extensions::EXTENSION_GROUP_INTERNAL_TRANSLATE_SCRIPTS,
      &results);
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

  v8::HandleScope handle_scope;
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      chrome::ISOLATED_WORLD_ID_TRANSLATE,
      &source,
      1,
      extensions::EXTENSION_GROUP_INTERNAL_TRANSLATE_SCRIPTS,
      &results);
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

  v8::HandleScope handle_scope;
  WebVector<v8::Local<v8::Value> > results;
  WebScriptSource source = WebScriptSource(ASCIIToUTF16(script));
  main_frame->executeScriptInIsolatedWorld(
      chrome::ISOLATED_WORLD_ID_TRANSLATE,
      &source,
      1,
      extensions::EXTENSION_GROUP_INTERNAL_TRANSLATE_SCRIPTS,
      &results);
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

  TranslateCommonMetrics::ReportUserActionDuration(language_determined_time_,
                                                   base::TimeTicks::Now());

  GURL url(main_frame->document().url());
  TranslateCommonMetrics::ReportPageScheme(url.scheme());

  // Set up v8 isolated world with proper content-security-policy and
  // security-origin.
  WebFrame* frame = GetMainFrame();
  if (frame) {
    frame->setIsolatedWorldContentSecurityPolicy(
        chrome::ISOLATED_WORLD_ID_TRANSLATE,
        WebString::fromUTF8(kContentSecurityPolicy));

    std::string security_origin(kSecurityOrigin);
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
      security_origin =
          command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
    }
    frame->setIsolatedWorldSecurityOrigin(
        chrome::ISOLATED_WORLD_ID_TRANSLATE,
        WebSecurityOrigin::create(GURL(security_origin)));
  }

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
    TranslateCommonMetrics::ReportTimeToTranslate(
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
  TranslateCommonMetrics::ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  TranslateCommonMetrics::ReportTimeToLoad(
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
