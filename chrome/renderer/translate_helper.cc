// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate_helper.h"

#include "base/compiler_specific.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptSource.h"

using WebKit::WebFrame;
using WebKit::WebScriptSource;

// The delay in millliseconds that we'll wait before checking to see if the
// translate library injected in the page is ready.
static const int kTranslateInitCheckDelayMs = 150;

// The maximum number of times we'll check to see if the translate library
// injected in the page is ready.
static const int kMaxTranslateInitCheckAttempts = 5;

// The delay we wait in milliseconds before checking whether the translation has
// finished.
static const int kTranslateStatusCheckDelayMs = 400;

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, public:
//
TranslateHelper::TranslateHelper(RenderView* render_view)
    : render_view_(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

void TranslateHelper::TranslatePage(int page_id,
                                    const std::string& source_lang,
                                    const std::string& target_lang,
                                    const std::string& translate_script) {
  if (render_view_->page_id() != page_id)
    return;  // We navigated away, nothing to do.

  if (!IsTranslateLibAvailable()) {
    // Evaluate the script to add the translation related method to the global
    // context of the page.
    ExecuteScript(translate_script);
    DCHECK(IsTranslateLibAvailable());
  }

  // Cancel any pending tasks related to a previous translation, they are now
  // obsolete.
  method_factory_.RevokeAll();

  TranslatePageImpl(page_id, source_lang, target_lang, 0);
}

void TranslateHelper::RevertTranslation(int page_id) {
  if (render_view_->page_id() != page_id)
    return;  // We navigated away, nothing to do.

  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    return;
  }

  WebFrame* main_frame = render_view_->webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->executeScript(
      WebScriptSource(ASCIIToUTF16("cr.googleTranslate.revert()")));
}

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

bool TranslateHelper::StartTranslation(const std::string& source_lang,
                                       const std::string& target_lang) {
  bool translate_success = false;
  if (!ExecuteScriptAndGetBoolResult("cr.googleTranslate.translate('" +
                                     source_lang + "','" + target_lang + "')",
                                     &translate_success)) {
    NOTREACHED();
    return false;
  }
  return translate_success;
}

////////////////////////////////////////////////////////////////////////////////
// TranslateHelper, private:
//
void TranslateHelper::CheckTranslateStatus(int page_id,
                                           const std::string& source_lang,
                                           const std::string& target_lang) {
  if (page_id != render_view_->page_id())
    return;  // This is not the same page, the translation has been canceled.

  // First check if there was an error.
  if (HasTranslationFailed()) {
    NotifyBrowserTranslationFailed(source_lang, target_lang,
                                   TranslateErrors::TRANSLATION_ERROR);
    return;  // There was an error.
  }

  if (HasTranslationFinished()) {
    // Translation was successfull, notify the browser.
    render_view_->Send(new ViewHostMsg_PageTranslated(
        render_view_->routing_id(), render_view_->page_id(),
        source_lang, target_lang, TranslateErrors::NONE));
    return;
  }

  // The translation is still pending, check again later.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&TranslateHelper::CheckTranslateStatus,
                                        page_id, source_lang, target_lang),
      DontDelayTasks() ? 0 : kTranslateStatusCheckDelayMs);
}

bool TranslateHelper::ExecuteScript(const std::string& script) {
  WebFrame* main_frame = render_view_->webview()->mainFrame();
  if (!main_frame)
    return false;
  main_frame->executeScript(WebScriptSource(ASCIIToUTF16(script)));
  return true;
}

bool TranslateHelper::ExecuteScriptAndGetBoolResult(const std::string& script,
                                                    bool* value) {
  DCHECK(value);
  WebFrame* main_frame = render_view_->webview()->mainFrame();
  if (!main_frame)
    return false;

  v8::Handle<v8::Value> v = main_frame->executeScriptAndReturnValue(
      WebScriptSource(ASCIIToUTF16(script)));
  if (v.IsEmpty() || !v->IsBoolean())
    return false;

  *value = v->BooleanValue();
  return true;
}

void TranslateHelper::TranslatePageImpl(int page_id,
                                        const std::string& source_lang,
                                        const std::string& target_lang,
                                        int count) {
  DCHECK_LT(count, kMaxTranslateInitCheckAttempts);
  if (page_id != render_view_->page_id())
    return;

  if (!IsTranslateLibReady()) {
    // The library is not ready, try again later, unless we have tried several
    // times unsucessfully already.
    if (++count >= kMaxTranslateInitCheckAttempts) {
      NotifyBrowserTranslationFailed(source_lang, target_lang,
                                     TranslateErrors::INITIALIZATION_ERROR);
      return;
    }
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&TranslateHelper::TranslatePageImpl,
                                          page_id, source_lang, target_lang,
                                          count),
        DontDelayTasks() ? 0 : count * kTranslateInitCheckDelayMs);
    return;
  }

  if (!StartTranslation(source_lang, target_lang)) {
    NotifyBrowserTranslationFailed(source_lang, target_lang,
                                   TranslateErrors::TRANSLATION_ERROR);
    return;
  }
  // Check the status of the translation.
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&TranslateHelper::CheckTranslateStatus,
                                        page_id, source_lang, target_lang),
      DontDelayTasks() ? 0 : kTranslateStatusCheckDelayMs);
}

void TranslateHelper::NotifyBrowserTranslationFailed(
    const std::string& source_lang,
    const std::string& target_lang,
    TranslateErrors::Type error) {
  // Notify the browser there was an error.
  render_view_->Send(new ViewHostMsg_PageTranslated(
      render_view_->routing_id(), render_view_->page_id(),
      source_lang, target_lang, error));
}
