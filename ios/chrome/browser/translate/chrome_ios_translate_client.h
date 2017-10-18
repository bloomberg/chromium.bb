// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_CHROME_IOS_TRANSLATE_CLIENT_H_
#define IOS_CHROME_BROWSER_TRANSLATE_CHROME_IOS_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/ios/browser/ios_translate_driver.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

class PrefService;

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {
class TranslateAcceptLanguages;
class TranslatePrefs;
class TranslateManager;

struct LanguageDetectionDetails;
}  // namespace translate

namespace web {
class WebState;
}  // namespace web

class ChromeIOSTranslateClient
    : public translate::TranslateClient,
      public web::WebStateObserver,
      public web::WebStateUserData<ChromeIOSTranslateClient> {
 public:
  ~ChromeIOSTranslateClient() override;

  // Helper method to return a new TranslatePrefs instance.
  static std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
      PrefService* prefs);

  // Gets the associated TranslateManager.
  translate::TranslateManager* GetTranslateManager();

  // TranslateClient implementation.
  translate::TranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  translate::TranslateAcceptLanguages* GetTranslateAcceptLanguages() override;
  int GetInfobarIconID() const override;
  // Record language detection event.
  void RecordLanguageDetectionEvent(
      const translate::LanguageDetectionDetails& details) const override;
  void RecordTranslateEvent(const metrics::TranslateEventProto&) override;
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

 private:
  explicit ChromeIOSTranslateClient(web::WebState* web_state);
  friend class web::WebStateUserData<ChromeIOSTranslateClient>;

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  std::unique_ptr<translate::TranslateManager> translate_manager_;
  translate::IOSTranslateDriver translate_driver_;

  DISALLOW_COPY_AND_ASSIGN(ChromeIOSTranslateClient);
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_CHROME_IOS_TRANSLATE_CLIENT_H_
