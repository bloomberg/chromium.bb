// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/translate/translate_prefs.h"
#include "chrome/common/translate_errors.h"

class SkBitmap;
class TranslateInfoBarView;

class TranslateInfoBarDelegate : public InfoBarDelegate {
 public:
  // The different types of infobars that can be shown for translation.
  enum Type {
    BEFORE_TRANSLATE,
    TRANSLATING,
    AFTER_TRANSLATE,
    TRANSLATION_ERROR
  };

  // The types of background color animations.
  enum BackgroundAnimationType {
    NONE,
    NORMAL_TO_ERROR,
    ERROR_TO_NORMAL
  };

  // Factory method to create a non-error translate infobar.
  // The original and target language specified are the ASCII language codes
  // (ex: en, fr...).
  // Returns NULL if it failed, typically if |original_language| or
  // |target_language| is not a supported language.
  static TranslateInfoBarDelegate* CreateDelegate(
      Type infobar_type,
      TabContents* tab_contents,
      const std::string& original_language,
      const std::string& target_language);

  // Factory method to create an error translate infobar.
  static TranslateInfoBarDelegate* CreateErrorDelegate(
      TranslateErrors::Type error_type,
      TabContents* tab_contents,
      const std::string& original_language,
      const std::string& target_language);

  virtual ~TranslateInfoBarDelegate();

  // Returns the number of languages supported.
  int GetLanguageCount() const { return static_cast<int>(languages_.size()); }

  // Returns the ISO code for the language at |index|.
  std::string GetLanguageCodeAt(int index) const;

  // Returns the displayable name for the language at |index|.
  string16 GetLanguageDisplayableNameAt(int index) const;

  TabContents* tab_contents() const { return tab_contents_; }

  Type type() const { return type_; }

  TranslateErrors::Type error() const { return error_; }

  int original_language_index() const { return original_language_index_; }
  int target_language_index() const { return target_language_index_; }

  // Convenience methods.
  std::string GetOriginalLanguageCode() const;
  std::string GetTargetLanguageCode() const;

  // Called by the InfoBar to notify that the original/target language has
  // changed and is now the language at |language_index|.
  virtual void SetOriginalLanguage(int language_index);
  virtual void SetTargetLanguage(int language_index);

  // Returns true if the current infobar indicates an error (in which case it
  // should get a yellow background instead of a blue one).
  bool IsError() const { return type_ == TRANSLATION_ERROR; }

  // Returns what kind of background fading effect the infobar should use when
  // its is shown.
  BackgroundAnimationType background_animation_type() const {
    return background_animation_;
  }

  virtual void Translate();
  virtual void RevertTranslation();
  virtual void ReportLanguageDetectionError();

  // Called when the user declines to translate a page, by either closing the
  // infobar or pressing the "Don't translate" button.
  void TranslationDeclined();

  // Methods called by the Options menu delegate.
  virtual bool IsLanguageBlacklisted();
  virtual void ToggleLanguageBlacklist();
  virtual bool IsSiteBlacklisted();
  virtual void ToggleSiteBlacklist();
  virtual bool ShouldAlwaysTranslate();
  virtual void ToggleAlwaysTranslate();

  // Methods called by the extra-buttons that can appear on the "before
  // translate" infobar (when the user has accepted/declined the translation
  // several times).
  virtual void AlwaysTranslatePageLanguage();
  virtual void NeverTranslatePageLanguage();

  // The following methods are called by the infobar that displays the status
  // while translating and also the one displaying the error message.
  string16 GetMessageInfoBarText();
  string16 GetMessageInfoBarButtonText();
  void MessageInfoBarButtonPressed();
  bool ShouldShowMessageInfoBarButton();

  // Called by the before translate infobar to figure-out if it should show
  // an extra button to let the user black-list/white-list that language (based
  // on how many times the user accepted/declined translation).
  bool ShouldShowNeverTranslateButton();
  bool ShouldShowAlwaysTranslateButton();

  // Sets this infobar background animation based on the previous infobar shown.
  // A fading background effect is used when transitioning from a normal state
  // to an error state (and vice-versa).
  void UpdateBackgroundAnimation(TranslateInfoBarDelegate* previous_infobar);

  // Convenience method that returns the displayable language name for
  // |language_code| in the current application locale.
  static string16 GetLanguageDisplayableName(const std::string& language_code);

  // Adds the strings that should be displayed in the after translate infobar to
  // |strings|. The text in that infobar is:
  // "The page has been translated from <lang1> to <lang2>."
  // Because <lang1> and <lang2> are displayed in menu buttons, the text is
  // split in 3 chunks.  |swap_languages| is set to true if <lang1> and <lang2>
  // should be inverted (some languages express the sentense as "The page has
  // been translate to <lang2> from <lang1>.").
  static void GetAfterTranslateStrings(std::vector<string16>* strings,
                                       bool* swap_languages);

 protected:
  // For testing.
  TranslateInfoBarDelegate(Type infobar_type,
                           TranslateErrors::Type error,
                           TabContents* tab_contents,
                           const std::string& original_language,
                           const std::string& target_language);
  Type type_;

 private:
  typedef std::pair<std::string, string16> LanguageNamePair;

  // InfoBarDelegate implementation:
  virtual InfoBar* CreateInfoBar();
  virtual void InfoBarDismissed();
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual InfoBarDelegate::Type GetInfoBarType() const;
  virtual TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();

  // Gets the host of the page being translated, or an empty string if no URL is
  // associated with the current page.
  std::string GetPageHost();

  // The type of fading animation if any that should be used when showing this
  // infobar.
  BackgroundAnimationType background_animation_;

  TabContents* tab_contents_;

  // The list supported languages for translation.
  // The pair first string is the language ISO code (ex: en, fr...), the second
  // string is the displayable name on the current locale.
  // The languages are sorted alphabetically based on the displayable name.
  std::vector<LanguageNamePair> languages_;

  // The index for language the page is originally in.
  int original_language_index_;

  // The index for language the page is originally in that was originally
  // reported (original_language_index_ changes if the user selects a new
  // original language, but this one does not).  This is necessary to report
  // language detection errors with the right original language even if the user
  // changed the original language.
  int initial_original_language_index_;

  // The index for language the page should be translated to.
  int target_language_index_;

  // The error that occurred when trying to translate (NONE if no error).
  TranslateErrors::Type error_;

  // The current infobar view.
  TranslateInfoBarView* infobar_view_;

  // The translation related preferences.
  TranslatePrefs prefs_;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_
