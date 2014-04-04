// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/translate_helper.h"

#if defined(CLD2_DYNAMIC_MODE)
#include <stdint.h>
#endif

#include "base/bind.h"
#include "base/compiler_specific.h"
#if defined(CLD2_DYNAMIC_MODE)
#include "base/files/memory_mapped_file.h"
#endif
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/isolated_world_ids.h"
#include "components/translate/content/common/translate_messages.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/language_detection/language_detection_util.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "ipc/ipc_platform_file.h"
#if defined(CLD2_DYNAMIC_MODE)
#include "content/public/common/url_constants.h"
#include "third_party/cld_2/src/public/compact_lang_det.h"
#endif
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

}  // namespace

#if defined(CLD2_DYNAMIC_MODE)
// The mmap for the CLD2 data must be held forever once it is available in the
// process. This is declared static in the translate_helper.h.
base::LazyInstance<TranslateHelper::CLDMmapWrapper>::Leaky
  TranslateHelper::s_cld_mmap_ = LAZY_INSTANCE_INITIALIZER;
#endif

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      page_id_(-1),
      translation_pending_(false),
      weak_method_factory_(this)
#if defined(CLD2_DYNAMIC_MODE)
      ,cld2_data_file_polling_started_(false),
      cld2_data_file_polling_canceled_(false),
      deferred_page_capture_(false),
      deferred_page_id_(-1),
      deferred_contents_(ASCIIToUTF16(""))
#endif
  {
}

TranslateHelper::~TranslateHelper() {
  CancelPendingTranslation();
#if defined(CLD2_DYNAMIC_MODE)
  CancelCLD2DataFilePolling();
#endif
}

void TranslateHelper::PrepareForUrl(const GURL& url) {
#if defined(CLD2_DYNAMIC_MODE)
  deferred_page_capture_ = false;
  deferred_contents_.clear();
  if (cld2_data_file_polling_started_)
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
  if (url.SchemeIs(content::kFtpScheme))
    return;
#if defined(OS_CHROMEOS)
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      url.DomainIs(file_manager::kFileManagerAppId))
    return;
#endif

  // Start polling for CLD data.
  cld2_data_file_polling_started_ = true;
  TranslateHelper::SendCLD2DataFileRequest(0, 1000);
#endif
}

#if defined(CLD2_DYNAMIC_MODE)
void TranslateHelper::DeferPageCaptured(const int page_id,
                                        const base::string16& contents) {
  deferred_page_capture_ = true;
  deferred_page_id_ = page_id;
  deferred_contents_ = contents;
}
#endif

void TranslateHelper::PageCaptured(int page_id,
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
  if (!main_frame || render_view()->GetPageId() != page_id)
    return;

  // TODO(andrewhayden): UMA insertion point here: Track if data is available.
  // TODO(andrewhayden): Retry insertion point here, retry till data available.
#if defined(CLD2_DYNAMIC_MODE)
  if (!CLD2::isDataLoaded()) {
    // We're in dynamic mode and CLD data isn't loaded. Retry when CLD data
    // is loaded, if ever.
    TranslateHelper::DeferPageCaptured(page_id, contents);
    return;
  }
#endif
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
  std::string language = translate::DeterminePageLanguage(
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
#if defined(CLD2_DYNAMIC_MODE)
  CancelCLD2DataFilePolling();
#endif
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

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
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

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
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

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
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
#if defined(CLD2_DYNAMIC_MODE)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_CLDDataAvailable, OnCLDDataAvailable);
#endif
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
  source_lang_ = (source_lang != translate::kUnknownLanguageCode) ?
                  source_lang : kAutoDetectionLanguage;
  target_lang_ = target_lang;

  translate::ReportUserActionDuration(language_determined_time_,
                                      base::TimeTicks::Now());

  GURL url(main_frame->document().url());
  translate::ReportPageScheme(url.scheme());

  // Set up v8 isolated world with proper content-security-policy and
  // security-origin.
  WebFrame* frame = GetMainFrame();
  if (frame) {
    frame->setIsolatedWorldContentSecurityPolicy(
        chrome::ISOLATED_WORLD_ID_TRANSLATE,
        WebString::fromUTF8(kContentSecurityPolicy));

    GURL security_origin = translate::GetTranslateSecurityOrigin();
    frame->setIsolatedWorldSecurityOrigin(
        chrome::ISOLATED_WORLD_ID_TRANSLATE,
        WebSecurityOrigin::create(security_origin));
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
    translate::ReportTimeToTranslate(
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
  translate::ReportTimeToBeReady(
      ExecuteScriptAndGetDoubleResult("cr.googleTranslate.readyTime"));
  translate::ReportTimeToLoad(
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

#if defined(CLD2_DYNAMIC_MODE)
void TranslateHelper::CancelCLD2DataFilePolling() {
  cld2_data_file_polling_canceled_ = true;
}

void TranslateHelper::SendCLD2DataFileRequest(const int delay_millis,
                                              const int next_delay_millis) {
  // Terminate immediately if told to stop polling.
  if (cld2_data_file_polling_canceled_)
    return;

  // Terminate immediately if data is already loaded.
  if (CLD2::isDataLoaded())
    return;

  // Else, send the IPC message to the browser process requesting the data...
  Send(new ChromeViewHostMsg_NeedCLDData(routing_id()));

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
      base::Bind(&TranslateHelper::SendCLD2DataFileRequest,
                 weak_method_factory_.GetWeakPtr(),
                 next_delay_millis, next_delay_millis),
      base::TimeDelta::FromMilliseconds(delay_millis));
}

void TranslateHelper::OnCLDDataAvailable(
    const IPC::PlatformFileForTransit ipc_file_handle,
    const uint64 data_offset,
    const uint64 data_length) {
  LoadCLDDData(ipc_file_handle, data_offset, data_length);
  if (deferred_page_capture_ && CLD2::isDataLoaded()) {
    deferred_page_capture_ = false; // Don't do this a second time.
    PageCaptured(deferred_page_id_, deferred_contents_);
    deferred_page_id_ = -1; // Clean up for sanity
    deferred_contents_.clear(); // Clean up for sanity
  }
}

void TranslateHelper::LoadCLDDData(
    const IPC::PlatformFileForTransit ipc_file_handle,
    const uint64 data_offset,
    const uint64 data_length) {
  // Terminate immediately if told to stop polling.
  if (cld2_data_file_polling_canceled_)
    return;

  // Terminate immediately if data is already loaded.
  if (CLD2::isDataLoaded())
    return;

  // Grab the file handle
  base::PlatformFile platform_file =
      IPC::PlatformFileForTransitToPlatformFile(ipc_file_handle);
  if (platform_file == base::kInvalidPlatformFileValue) {
    LOG(ERROR) << "Can't find the CLD data file.";
    return;
  }
  base::File basic_file(platform_file);

  // mmap the file
  s_cld_mmap_.Get().value = new base::MemoryMappedFile();
  bool initialized = s_cld_mmap_.Get().value->Initialize(basic_file.Pass());
  if (!initialized) {
    LOG(ERROR) << "mmap initialization failed";
    delete s_cld_mmap_.Get().value;
    s_cld_mmap_.Get().value = NULL;
    return;
  }

  // Sanity checks
  uint64 max_int32 = std::numeric_limits<int32>::max();
  if (data_length + data_offset > s_cld_mmap_.Get().value->length()
      || data_length > max_int32) { // max signed 32 bit integer
    LOG(ERROR) << "Illegal mmap config: data_offset="
        << data_offset << ", data_length=" << data_length
        << ", mmap->length()=" << s_cld_mmap_.Get().value->length();
    delete s_cld_mmap_.Get().value;
    s_cld_mmap_.Get().value = NULL;
    return;
  }

  // Initialize the CLD subsystem... and it's all done!
  const uint8* data_ptr = s_cld_mmap_.Get().value->data() + data_offset;
  CLD2::loadDataFromRawAddress(data_ptr, data_length);
  DCHECK(CLD2::isDataLoaded()) << "Failed to load CLD data from mmap";
}
#endif
