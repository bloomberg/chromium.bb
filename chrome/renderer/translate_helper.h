// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_HELPER_H_
#define CHROME_RENDERER_TRANSLATE_HELPER_H_

#include <string>

#include "base/task.h"
#include "chrome/common/translate_errors.h"

class RenderView;

// This class deals with page translation.
// There is one TranslateHelper per RenderView.

class TranslateHelper {
 public:
  explicit TranslateHelper(RenderView* render_view);
  virtual ~TranslateHelper() {}

  // Translates the page contents from |source_lang| to |target_lang|.
  // Does nothing if |page_id| is not the current page id.
  // If the library is not ready, it will post a task to try again after 50ms.
  void TranslatePage(int page_id,
                     const std::string& source_lang,
                     const std::string& target_lang,
                     const std::string& translate_script);

  // Reverts the page's text to its original contents.
  void RevertTranslation(int page_id);

 protected:
  // The following methods are protected so they can be overridden in
  // unit-tests.

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
  virtual bool StartTranslation(const std::string& original_lang,
                                const std::string& target_lang);

  // Used in unit-tests. Makes the various tasks be posted immediately so that
  // the tests don't have to wait before checking states.
  virtual bool DontDelayTasks() { return false; }

 private:
  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later.
  void CheckTranslateStatus(int page_id,
                            const std::string& source_lang,
                            const std::string& target_lang);

  // Executes the JavaScript code in |script| in the main frame of
  // |render_view_host_|.
  // Returns true if the code was executed successfully.
  bool ExecuteScript(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of
  // |render_view_host_|, and sets |value| to the boolean returned by the script
  // evaluation.  Returns true if the script was run successfully and returned
  // a boolean, false otherwise
  bool ExecuteScriptAndGetBoolResult(const std::string& script, bool* value);

  // Called by TranslatePage to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslatePageImpl(int page_id,
                         const std::string& source_lang,
                         const std::string& target_lang,
                         int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(const std::string& original_lang,
                                      const std::string& target_lang,
                                      TranslateErrors::Type error);

  // The RenderView we are performing translations for.
  RenderView* render_view_;

  // Method factory used to make calls to TranslatePageImpl.
  ScopedRunnableMethodFactory<TranslateHelper> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateHelper);
};

#endif  // CHROME_RENDERER_TRANSLATE_HELPER_H_
