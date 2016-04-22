// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_HELPER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_HELPER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

namespace blink {
class WebDocument;
class WebLocalFrame;
}

namespace content {
class RendererCldDataProvider;
}

namespace translate {

// This class deals with page translation.
// There is one TranslateHelper per RenderView.
//
// This class provides metrics that allow tracking the user experience impact
// of non-static CldDataProvider implementations. For background on the data
// providers, please refer to the following documentation:
// http://www.chromium.org/developers/how-tos/compact-language-detector-cld-data-source-configuration
//
// Available metrics (from the LanguageDetectionTiming enum):
// 1. ON_TIME
//    Recorded if PageCaptured(...) is invoked after CLD is available. This is
//    the ideal case, indicating that CLD is available before it is needed.
// 2. DEFERRED
//    Recorded if PageCaptured(...) is invoked before CLD is available.
//    Sub-optimal case indicating that CLD wasn't available when it was needed,
//    so the request for detection has been deferred until CLD is available or
//    until the user navigates to a different page.
// 3. RESUMED
//    Recorded if CLD becomes available after a language detection request was
//    deferred, but before the user navigated to a different page. Language
//    detection is ultimately completed, it just didn't happen on time.
//
// Note that there is NOT a metric that records the number of times that
// language detection had to be aborted because CLD never became available in
// time. This is because there is no reasonable way to cover all the cases
// under which this could occur, particularly the destruction of the renderer
// for which this object was created. However, this value can be synthetically
// derived, using the logic below.
//
// Every page load that triggers language detection will result in the
// recording of exactly one of the first two events: ON_TIME or DEFERRED. If
// CLD is available in time to satisfy the request, the third event (RESUMED)
// will be recorded; thus, the number of times when language detection
// ultimately fails because CLD isn't ever available is implied as the number of
// times that detection is deferred minus the number of times that language
// detection is late:
//
//   count(FAILED) ~= count(DEFERRED) - count(RESUMED)
//
// Note that this is not 100% accurate: some renderer process are so short-lived
// that language detection wouldn't have been relevant anyway, and so a failure
// to detect the language in a timely manner might be completely innocuous. The
// overall problem with language detection is that it isn't possible to know
// whether it was required or not until after it has been performed!
//
// We use histograms for recording these metrics. On Android, the renderer can
// be killed without the chance to clean up or transmit these histograms,
// leading to dropped metrics. To work around this, this method forces an IPC
// message to be sent to the browser process immediately.
class TranslateHelper : public content::RenderFrameObserver {
 public:
  explicit TranslateHelper(content::RenderFrame* render_frame,
                           int world_id,
                           int extension_group,
                           const std::string& extension_scheme);
  ~TranslateHelper() override;

  // Informs us that the page's text has been extracted.
  void PageCaptured(const base::string16& contents);

  // Lets the translation system know that we are preparing to navigate to
  // the specified URL. If there is anything that can or should be done before
  // this URL loads, this is the time to prepare for it.
  void PrepareForUrl(const GURL& url);

 protected:
  // The following methods are protected so they can be overridden in
  // unit-tests.
  void OnTranslatePage(int page_seq_no,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);
  void OnRevertTranslation(int page_seq_no);

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
  enum LanguageDetectionTiming {
    ON_TIME,   // Language detection was performed as soon as it was requested
    DEFERRED,  // Language detection couldn't be performed when it was requested
    RESUMED,   // A deferred language detection attempt was completed later
    LANGUAGE_DETECTION_TIMING_MAX_VALUE  // The bounding value for this enum
  };

  // Converts language code to the one used in server supporting list.
  static void ConvertLanguageCodeSynonym(std::string* code);

  // RenderFrameObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // Informs us that the page's text has been extracted.
  void PageCapturedImpl(int page_seq_no, const base::string16& contents);

  // Cancels any translation that is currently being performed.  This does not
  // revert existing translations.
  void CancelPendingTranslation();

  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later.
  void CheckTranslateStatus(int page_seq_no);

  // Called by TranslatePage to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslatePageImpl(int page_seq_no, int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(TranslateErrors::Type error);

  // Convenience method to access the main frame.  Can return NULL, typically
  // if the page is being closed.
  blink::WebLocalFrame* GetMainFrame();

  // Do not ask for CLD data any more.
  void CancelCldDataPolling();

  // Start polling for CLD data.
  // Polling will automatically halt as soon as the renderer obtains a
  // reference to the data file.
  void SendCldDataRequest(const int delay_millis, const int next_delay_millis);

  // Callback triggered when CLD data becomes available.
  void OnCldDataAvailable();

  // Record the timing of language detection, immediately sending an IPC-based
  // histogram delta update to the browser process in case the hosting renderer
  // process terminates before the metrics would otherwise be transferred.
  void RecordLanguageDetectionTiming(LanguageDetectionTiming timing);

  // An ever-increasing sequence number of the current page, used to match up
  // translation requests with responses.
  int page_seq_no_;

  // The states associated with the current translation.
  bool translation_pending_;
  std::string source_lang_;
  std::string target_lang_;

  // Time when a page langauge is determined. This is used to know a duration
  // time from showing infobar to requesting translation.
  base::TimeTicks language_determined_time_;

  // Provides CLD data for this process.
  std::unique_ptr<RendererCldDataProvider> cld_data_provider_;

  // Whether or not polling for CLD2 data has started.
  bool cld_data_polling_started_;

  // Whether or not CancelCldDataPolling has been called.
  bool cld_data_polling_canceled_;

  // Whether or not a PageCaptured event arrived prior to CLD data becoming
  // available. If true, deferred_contents_ contains the most recent contents.
  bool deferred_page_capture_;

  // The ID of the page most recently reported to PageCaptured if
  // deferred_page_capture_ is true.
  int deferred_page_seq_no_;

  // The world ID to use for script execution.
  int world_id_;

  // The extension group.
  int extension_group_;

  // The URL scheme for translate extensions.
  std::string extension_scheme_;

  // The contents of the page most recently reported to PageCaptured if
  // deferred_page_capture_ is true.
  base::string16 deferred_contents_;

  // Method factory used to make calls to TranslatePageImpl.
  base::WeakPtrFactory<TranslateHelper> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateHelper);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_HELPER_H_
