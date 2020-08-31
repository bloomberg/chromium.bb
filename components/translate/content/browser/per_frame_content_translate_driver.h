// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_PER_FRAME_CONTENT_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_PER_FRAME_CONTENT_TRANSLATE_DRIVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/services/language_detection/public/cpp/language_detection_service.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {
class NavigationController;
class WebContents;
}  // namespace content

namespace language {
class UrlLanguageHistogram;
}  // namespace language

namespace translate {

struct LanguageDetectionDetails;

// Content implementation of TranslateDriver that support translation of sub
// frames.
class PerFrameContentTranslateDriver : public ContentTranslateDriver {
 public:
  PerFrameContentTranslateDriver(
      content::NavigationController* nav_controller,
      language::UrlLanguageHistogram* url_language_histogram);
  ~PerFrameContentTranslateDriver() override;

  // TranslateDriver methods.
  void TranslatePage(int page_seq_no,
                     const std::string& translate_script,
                     const std::string& source_lang,
                     const std::string& target_lang) override;
  void RevertTranslation(int page_seq_no) override;

  // content::WebContentsObserver implementation.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;
  void DocumentOnLoadCompletedInMainFrame() override;

  void OnPageLanguageDetermined(const LanguageDetectionDetails& details,
                                bool page_needs_translation);

 private:
  friend class PerFrameContentTranslateDriverTest;
  friend class TranslateManagerRenderViewHostTest;

  struct PendingRequestStats {
   public:
    PendingRequestStats();
    ~PendingRequestStats();

    void Clear();
    void Report();

    int pending_request_count = 0;
    bool main_frame_success = false;
    int frame_request_count = 0;
    int frame_success_count = 0;
    TranslateErrors::Type main_frame_error = TranslateErrors::NONE;
    std::vector<TranslateErrors::Type> frame_errors;
  };

  void StartLanguageDetection();

  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      content::RenderFrameHost* render_frame_host);

  void RevertFrame(content::RenderFrameHost* render_frame_host);

  // Callback for the GetWebLanguageDetectionDetails IPC. Note we will
  // pass the Remote handle to the TranslateAgent through to it in order
  // for the Remote handle to stay alive to receive the callback.
  void OnWebLanguageDetectionDetails(
      mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent,
      const std::string& content_language,
      const std::string& html_lang,
      const GURL& url,
      bool has_no_translate_meta);

  void OnPageContents(base::TimeTicks capture_begin_time,
                      const base::string16& contents);

  void OnPageContentsLanguage(
      mojo::Remote<language_detection::mojom::LanguageDetectionService>
          service_handle,
      const std::string& contents_language,
      bool is_contents_language_reliable);

  void ComputeActualPageLanguage();

  // Callback for the TranslateFrame IPC. Note we will  pass the Remote
  // handle to the TranslateAgent through to it in order for the Remote
  // handle to stay alive to receive the callback.
  void OnFrameTranslated(
      bool is_main_frame,
      mojo::AssociatedRemote<mojom::TranslateAgent> translate_agent,
      bool cancelled,
      const std::string& original_lang,
      const std::string& translated_lang,
      TranslateErrors::Type error_type);

  bool IsForCurrentPage(int page_seq_no);

  // Sequence number to track most recent main frame for associated WebContents.
  int current_seq_no_ = 0;

  LanguageDetectionDetails details_;

  bool awaiting_contents_ = false;

  // Time when a page language is determined. This is used to know a duration
  // time from showing infobar to requesting translation.
  base::TimeTicks language_determined_time_;

  PendingRequestStats stats_;

  base::WeakPtrFactory<PerFrameContentTranslateDriver> weak_pointer_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PerFrameContentTranslateDriver);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_PER_FRAME_CONTENT_TRANSLATE_DRIVER_H_
