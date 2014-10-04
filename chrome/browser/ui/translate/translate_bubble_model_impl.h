// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"

namespace translate {
class TranslateUIDelegate;
}

// The standard implementation of TranslateBubbleModel.
class TranslateBubbleModelImpl : public TranslateBubbleModel {
 public:
  TranslateBubbleModelImpl(
      translate::TranslateStep step,
      scoped_ptr<translate::TranslateUIDelegate> ui_delegate);
  virtual ~TranslateBubbleModelImpl();

  // Converts a TranslateStep to a ViewState.
  // This function never returns VIEW_STATE_ADVANCED.
  static TranslateBubbleModel::ViewState TranslateStepToViewState(
      translate::TranslateStep step);

  // TranslateBubbleModel methods.
  virtual TranslateBubbleModel::ViewState GetViewState() const override;
  virtual void SetViewState(TranslateBubbleModel::ViewState view_state)
      override;
  virtual void ShowError(translate::TranslateErrors::Type error_type) override;
  virtual void GoBackFromAdvanced() override;
  virtual int GetNumberOfLanguages() const override;
  virtual base::string16 GetLanguageNameAt(int index) const override;
  virtual int GetOriginalLanguageIndex() const override;
  virtual void UpdateOriginalLanguageIndex(int index) override;
  virtual int GetTargetLanguageIndex() const override;
  virtual void UpdateTargetLanguageIndex(int index) override;
  virtual void SetNeverTranslateLanguage(bool value) override;
  virtual void SetNeverTranslateSite(bool value) override;
  virtual bool ShouldAlwaysTranslate() const override;
  virtual void SetAlwaysTranslate(bool value) override;
  virtual void Translate() override;
  virtual void RevertTranslation() override;
  virtual void TranslationDeclined(bool explicitly_closed) override;
  virtual bool IsPageTranslatedInCurrentLanguages() const override;

 private:
  scoped_ptr<translate::TranslateUIDelegate> ui_delegate_;
  TranslateBubbleViewStateTransition view_state_transition_;

  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleModelImpl);
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_IMPL_H_
