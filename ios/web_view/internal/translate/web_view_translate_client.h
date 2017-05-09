// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#import "base/ios/weak_nsobject.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#import "components/translate/ios/browser/ios_translate_driver.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class CWVTranslationController;

class PrefService;

namespace translate {
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;
}  // namespace translate

namespace web {
class WebState;
}

namespace ios_web_view {

class WebViewTranslateClient
    : public translate::TranslateClient,
      public web::WebStateObserver,
      public web::WebStateUserData<WebViewTranslateClient> {
 public:
  ~WebViewTranslateClient() override;

  // This |controller| is assumed to outlive this WebViewTranslateClient.
  void set_translation_controller(CWVTranslationController* controller) {
    translation_controller_.reset(controller);
  }

  translate::TranslateManager* translate_manager() {
    return translate_manager_.get();
  }

 private:
  friend class web::WebStateUserData<WebViewTranslateClient>;

  // The lifetime of WebViewTranslateClient is managed by WebStateUserData.
  explicit WebViewTranslateClient(web::WebState* web_state);

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages() override;
  int GetInfobarIconID() const override;
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
      const override;
  void ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors::Type error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;
  void ShowReportLanguageDetectionErrorUI(const GURL& report_url) override;

  // web::WebStateObserver implementation.
  void WebStateDestroyed() override;

  std::unique_ptr<translate::TranslateManager> translate_manager_;
  translate::IOSTranslateDriver translate_driver_;

  // ObjC class that wraps this class.
  base::WeakNSObject<CWVTranslationController> translation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTranslateClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_
