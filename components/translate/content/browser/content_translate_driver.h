// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationController;
class WebContents;
}

namespace translate {

struct LanguageDetectionDetails;
class TranslateManager;

// Content implementation of TranslateDriver.
class ContentTranslateDriver : public TranslateDriver,
                               public content::WebContentsObserver {
 public:

  // The observer for the ContentTranslateDriver.
  class Observer {
   public:
    // Handles when the value of IsPageTranslated is changed.
    virtual void OnIsPageTranslatedChanged(content::WebContents* source) {};

    // Handles when the value of translate_enabled is changed.
    virtual void OnTranslateEnabledChanged(content::WebContents* source) {};

    // Called when the page language has been determined.
    virtual void OnLanguageDetermined(
        const translate::LanguageDetectionDetails& details) {};

    // Called when the page has been translated.
    virtual void OnPageTranslated(
        const std::string& original_lang,
        const std::string& translated_lang,
        translate::TranslateErrors::Type error_type) {};

   protected:
    virtual ~Observer() {}
  };

  ContentTranslateDriver(content::NavigationController* nav_controller);
  virtual ~ContentTranslateDriver();

  // Adds or Removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attempts(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // Sets the TranslateManager associated with this driver.
  void set_translate_manager(TranslateManager* manager) {
    translate_manager_ = manager;
  }

  // Initiates translation once the page is finished loading.
  void InitiateTranslation(const std::string& page_lang, int attempt);

  // TranslateDriver methods.
  virtual void OnIsPageTranslatedChanged() OVERRIDE;
  virtual void OnTranslateEnabledChanged() OVERRIDE;
  virtual bool IsLinkNavigation() OVERRIDE;
  virtual void TranslatePage(int page_seq_no,
                             const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang) OVERRIDE;
  virtual void RevertTranslation(int page_seq_no) OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual const std::string& GetContentsMimeType() OVERRIDE;
  virtual const GURL& GetLastCommittedURL() OVERRIDE;
  virtual const GURL& GetActiveURL() OVERRIDE;
  virtual const GURL& GetVisibleURL() OVERRIDE;
  virtual bool HasCurrentPage() OVERRIDE;
  virtual void OpenUrlInNewTab(const GURL& url) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE;
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC handlers.
  void OnTranslateAssignedSequenceNumber(int page_seq_no);
  void OnLanguageDetermined(const LanguageDetectionDetails& details,
                            bool page_needs_translation);
  void OnPageTranslated(const std::string& original_lang,
                        const std::string& translated_lang,
                        TranslateErrors::Type error_type);

 private:
  // The navigation controller of the tab we are associated with.
  content::NavigationController* navigation_controller_;

  TranslateManager* translate_manager_;

  ObserverList<Observer, true> observer_list_;

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  base::WeakPtrFactory<ContentTranslateDriver> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentTranslateDriver);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
