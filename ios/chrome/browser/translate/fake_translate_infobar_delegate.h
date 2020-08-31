// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_

#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_infobar_delegate.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"

// Fake of TranslateInfoBarDelegate that allows for triggering Observer
// callbacks.
class FakeTranslateInfoBarDelegate
    : public translate::TranslateInfoBarDelegate {
 public:
  FakeTranslateInfoBarDelegate(
      const base::WeakPtr<translate::TranslateManager>& translate_manager,
      bool is_off_the_record,
      translate::TranslateStep step,
      const std::string& original_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool triggered_from_menu);
  ~FakeTranslateInfoBarDelegate() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Call the OnTranslateStepChanged() observer method on all
  // |OnTranslateStepChanged|.
  void TriggerOnTranslateStepChanged(
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type);

 private:
  base::ObserverList<Observer> observers_;
};

// Factory class to create instances of FakeTranslateInfoBarDelegate.
class FakeTranslateInfoBarDelegateFactory {
 public:
  FakeTranslateInfoBarDelegateFactory();
  ~FakeTranslateInfoBarDelegateFactory();

  // Create a FakeTranslateInfoBarDelegate unique_ptr with
  // |original_language|and |target_language|.
  std::unique_ptr<FakeTranslateInfoBarDelegate>
  CreateFakeTranslateInfoBarDelegate(const std::string& original_language,
                                     const std::string& target_language);

 private:
  translate::testing::MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<translate::testing::MockTranslateClient> client_;
  std::unique_ptr<translate::testing::MockTranslateRanker> ranker_;
  std::unique_ptr<translate::testing::MockLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> manager_;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_FAKE_TRANSLATE_INFOBAR_DELEGATE_H_
