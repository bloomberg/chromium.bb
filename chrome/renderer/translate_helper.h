// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_HELPER_H_
#define CHROME_RENDERER_TRANSLATE_HELPER_H_
#pragma once

#include <string>

#include "base/task.h"
#include "chrome/common/translate_errors.h"
#include "chrome/renderer/render_view_observer.h"

class RenderView;
namespace WebKit {
class WebDocument;
class WebFrame;
}

// This class deals with page translation.
// There is one TranslateHelper per RenderView.

class TranslateHelper : public RenderViewObserver {
 public:
  explicit TranslateHelper(RenderView* render_view);
  virtual ~TranslateHelper();

  // Returns whether the page associated with |document| is a candidate for
  // translation.  Some pages can explictly specify (via a meta-tag) that they
  // should not be translated.
  static bool IsPageTranslatable(WebKit::WebDocument* document);

  // Returns the language specified in the language meta tag of |document|, or
  // an empty string if no such tag was found.
  // The tag may specify several languages, the first one is returned.
  // Example of such meta-tag:
  // <meta http-equiv="content-language" content="en, fr">
  static std::string GetPageLanguageFromMetaTag(WebKit::WebDocument* document);

 protected:
  // The following methods are protected so they can be overridden in
  // unit-tests.
  void OnTranslatePage(int page_id,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);
  void OnRevertTranslation(int page_id);

  // Returns true if the translate library is available, meaning the JavaScript
  // has already been injected in that page.
  virtual bool IsTranslateLibAvailable();

  // Returns true if the translate library has been initialized successfully.
  virtual bool IsTranslateLibReady();

  // Returns true if the translation script has finished translating the page.
  virtual bool HasTranslationFinished();

  // Returns true if the translation script has reported an error performing the
  // translation.
  virtual bool HasTranslationFailed();

  // Starts the translation by calling the translate library.  This method
  // should only be called when the translate script has been injected in the
  // page.  Returns false if the call failed immediately.
  virtual bool StartTranslation();

  // Asks the Translate element in the page what the language of the page is.
  // Can only be called if a translation has happened and was successful.
  // Returns the language code on success, an empty string on failure.
  virtual std::string GetOriginalPageLanguage();

  // Used in unit-tests. Makes the various tasks be posted immediately so that
  // the tests don't have to wait before checking states.
  virtual bool DontDelayTasks();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Cancels any translation that is currently being performed.  This does not
  // revert existing translations.
  void CancelPendingTranslation();

  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later.
  void CheckTranslateStatus();

  // Executes the JavaScript code in |script| in the main frame of
  // |render_view_host_|.
  // Returns true if the code was executed successfully.
  bool ExecuteScript(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of
  // |render_view_host_|, and sets |value| to the boolean returned by the script
  // evaluation.  Returns true if the script was run successfully and returned
  // a boolean, false otherwise
  bool ExecuteScriptAndGetBoolResult(const std::string& script, bool* value);

  // Executes the JavaScript code in |script| in the main frame of
  // |render_view_host_|, and sets |value| to the string returned by the script
  // evaluation.  Returns true if the script was run successfully and returned
  // a string, false otherwise
  bool ExecuteScriptAndGetStringResult(const std::string& script,
                                       std::string* value);

  // Called by TranslatePage to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslatePageImpl(int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(TranslateErrors::Type error);

  // Convenience method to access the main frame.  Can return NULL, typically
  // if the page is being closed.
  WebKit::WebFrame* GetMainFrame();

  // The RenderView we are performing translations for.
  RenderView* render_view_;

  // The states associated with the current translation.
  bool translation_pending_;
  int page_id_;
  std::string source_lang_;
  std::string target_lang_;

  // Method factory used to make calls to TranslatePageImpl.
  ScopedRunnableMethodFactory<TranslateHelper> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateHelper);
};

#endif  // CHROME_RENDERER_TRANSLATE_HELPER_H_
