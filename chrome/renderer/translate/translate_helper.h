// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_H_
#define CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/renderer/render_view_observer.h"

#if defined(CLD2_DYNAMIC_MODE)
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "ipc/ipc_platform_file.h"
#include "url/gurl.h"
#endif

namespace blink {
class WebDocument;
class WebFrame;
}

// This class deals with page translation.
// There is one TranslateHelper per RenderView.

class TranslateHelper : public content::RenderViewObserver {
 public:
  explicit TranslateHelper(content::RenderView* render_view);
  virtual ~TranslateHelper();

  // Informs us that the page's text has been extracted.
  void PageCaptured(int page_id, const base::string16& contents);

  // Lets the translation system know that we are preparing to navigate to
  // the specified URL. If there is anything that can or should be done before
  // this URL loads, this is the time to prepare for it.
  void PrepareForUrl(const GURL& url);

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

  // Adjusts a delay time for a posted task. This is used in tests to do tasks
  // immediately by returning 0.
  virtual base::TimeDelta AdjustDelay(int delayInMs);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  virtual void ExecuteScript(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the boolean returned by the script evaluation if the script was
  // run successfully. Otherwise, returns |fallback| value.
  virtual bool ExecuteScriptAndGetBoolResult(const std::string& script,
                                             bool fallback);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the string returned by the script evaluation if the script was
  // run successfully. Otherwise, returns empty string.
  virtual std::string ExecuteScriptAndGetStringResult(
      const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  // and returns the number returned by the script evaluation if the script was
  // run successfully. Otherwise, returns 0.0.
  virtual double ExecuteScriptAndGetDoubleResult(const std::string& script);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, AdoptHtmlLang);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest,
                           CLDAgreeWithLanguageCodeHavingCountryCode);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest,
                           CLDDisagreeWithWrongLanguageCode);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest,
                           InvalidLanguageMetaTagProviding);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, LanguageCodeTypoCorrection);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, LanguageCodeSynonyms);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, ResetInvalidLanguageCode);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, SimilarLanguageCode);
  FRIEND_TEST_ALL_PREFIXES(TranslateHelperTest, WellKnownWrongConfiguration);

  // Converts language code to the one used in server supporting list.
  static void ConvertLanguageCodeSynonym(std::string* code);

  // Returns whether the page associated with |document| is a candidate for
  // translation.  Some pages can explictly specify (via a meta-tag) that they
  // should not be translated.
  static bool IsTranslationAllowed(blink::WebDocument* document);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Cancels any translation that is currently being performed.  This does not
  // revert existing translations.
  void CancelPendingTranslation();

  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later.
  void CheckTranslateStatus();

  // Called by TranslatePage to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslatePageImpl(int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(TranslateErrors::Type error);

  // Convenience method to access the main frame.  Can return NULL, typically
  // if the page is being closed.
  blink::WebFrame* GetMainFrame();

  // ID to represent a page which TranslateHelper captured and determined a
  // content language.
  int page_id_;

  // The states associated with the current translation.
  bool translation_pending_;
  std::string source_lang_;
  std::string target_lang_;

  // Time when a page langauge is determined. This is used to know a duration
  // time from showing infobar to requesting translation.
  base::TimeTicks language_determined_time_;

  // Method factory used to make calls to TranslatePageImpl.
  base::WeakPtrFactory<TranslateHelper> weak_method_factory_;

#if defined(CLD2_DYNAMIC_MODE)
  // Do not ask for CLD data any more.
  void CancelCLD2DataFilePolling();

  // Invoked when PageCaptured is called prior to obtaining CLD data. This
  // method stores the page ID into deferred_page_id_ and COPIES the contents
  // of the page, then sets deferred_page_capture_ to true. When CLD data is
  // eventually received (in OnCLDDataAvailable), any deferred request will be
  // "resurrected" and allowed to proceed automatically, assuming that the
  // page ID has not changed.
  void DeferPageCaptured(const int page_id, const base::string16& contents);

  // Immediately send an IPC request to the browser process to get the CLD
  // data file. In most cases, the file will already exist and we will only
  // poll once; but since the file might need to be downloaded first, poll
  // indefinitely until a ChromeViewMsg_CLDDataAvailable message is received
  // from the browser process.
  // Polling will automatically halt as soon as the renderer obtains a
  // reference to the data file.
  void SendCLD2DataFileRequest(const int delay_millis,
                               const int next_delay_millis);

  // Invoked when a ChromeViewMsg_CLDDataAvailable message is received from
  // the browser process, providing a file handle for the CLD data file. If a
  // PageCaptured request was previously deferred with DeferPageCaptured and
  // the page ID has not yet changed, the PageCaptured is reinvoked to
  // "resurrect" the language detection pathway.
  void OnCLDDataAvailable(const IPC::PlatformFileForTransit ipc_file_handle,
                          const uint64 data_offset,
                          const uint64 data_length);

  // After receiving data in OnCLDDataAvailable, loads the data into CLD2.
  void LoadCLDDData(base::File file,
                    const uint64 data_offset,
                    const uint64 data_length);

  // A struct that contains the pointer to the CLD mmap. Used so that we can
  // leverage LazyInstance:Leaky to properly scope the lifetime of the mmap.
  struct CLDMmapWrapper {
    CLDMmapWrapper() {
      value = NULL;
    }
    base::MemoryMappedFile* value;
  };
  static base::LazyInstance<CLDMmapWrapper>::Leaky s_cld_mmap_;

  // Whether or not polling for CLD2 data has started.
  bool cld2_data_file_polling_started_;

  // Whether or not CancelCLD2DataFilePolling has been called.
  bool cld2_data_file_polling_canceled_;

  // Whether or not a PageCaptured event arrived prior to CLD data becoming
  // available. If true, deferred_page_id_ contains the most recent page ID
  // and deferred_contents_ contains the most recent contents.
  bool deferred_page_capture_;

  // The ID of the page most recently reported to PageCaptured if
  // deferred_page_capture_ is true.
  int deferred_page_id_;

  // The contents of the page most recently reported to PageCaptured if
  // deferred_page_capture_ is true.
  base::string16 deferred_contents_;

#endif

  DISALLOW_COPY_AND_ASSIGN(TranslateHelper);
};

#endif  // CHROME_RENDERER_TRANSLATE_TRANSLATE_HELPER_H_
