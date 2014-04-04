// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/translate/translate_ui_delegate.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"

class PrefService;

class TranslateInfoBarDelegate : public InfoBarDelegate {
 public:
  // The types of background color animations.
  enum BackgroundAnimationType {
    NONE,
    NORMAL_TO_ERROR,
    ERROR_TO_NORMAL
  };

  static const size_t kNoIndex;

  virtual ~TranslateInfoBarDelegate();

  // Factory method to create a translate infobar.  |error_type| must be
  // specified iff |step| == TRANSLATION_ERROR.  For other translate steps,
  // |original_language| and |target_language| must be ASCII language codes
  // (e.g. "en", "fr", etc.) for languages the TranslateManager supports
  // translating.  The lone exception is when the user initiates translation
  // from the context menu, in which case it's legal to call this with
  // |step| == TRANSLATING and |original_language| == kUnknownLanguageCode.
  //
  // If |replace_existing_infobar| is true, the infobar is created and added to
  // the infobar service for |web_contents|, replacing any other translate
  // infobar already present there.  Otherwise, the infobar will only be added
  // if there is no other translate infobar already present.
  static void Create(bool replace_existing_infobar,
                     content::WebContents* web_contents,
                     TranslateTabHelper::TranslateStep step,
                     const std::string& original_language,
                     const std::string& target_language,
                     TranslateErrors::Type error_type,
                     PrefService* prefs,
                     bool triggered_from_menu);

  // Returns the number of languages supported.
  size_t num_languages() const { return ui_delegate_.GetNumberOfLanguages(); }

  // Returns the ISO code for the language at |index|.
  std::string language_code_at(size_t index) const {
    return ui_delegate_.GetLanguageCodeAt(index);
  }

  // Returns the displayable name for the language at |index|.
  base::string16 language_name_at(size_t index) const {
    return ui_delegate_.GetLanguageNameAt(index);
  }

  TranslateTabHelper::TranslateStep translate_step() const { return step_; }

  TranslateErrors::Type error_type() const { return error_type_; }

  size_t original_language_index() const {
    return ui_delegate_.GetOriginalLanguageIndex();
  }
  void UpdateOriginalLanguageIndex(size_t language_index);

  size_t target_language_index() const {
    return ui_delegate_.GetTargetLanguageIndex();
  }
  void UpdateTargetLanguageIndex(size_t language_index);

  // Convenience methods.
  std::string original_language_code() const {
    return ui_delegate_.GetOriginalLanguageCode();
  }
  std::string target_language_code() const {
    return ui_delegate_.GetTargetLanguageCode();
  }

  // Returns true if the current infobar indicates an error (in which case it
  // should get a yellow background instead of a blue one).
  bool is_error() const { return step_ == TranslateTabHelper::TRANSLATE_ERROR; }


  // Return true if the translation was triggered by a menu entry instead of
  // via an infobar/bubble or preference.
  bool triggered_from_menu() const {
    return triggered_from_menu_;
  }

  // Returns what kind of background fading effect the infobar should use when
  // its is shown.
  BackgroundAnimationType background_animation_type() const {
    return background_animation_;
  }

  virtual void Translate();
  virtual void RevertTranslation();
  void ReportLanguageDetectionError();

  // Called when the user declines to translate a page, by either closing the
  // infobar or pressing the "Don't translate" button.
  virtual void TranslationDeclined();

  // Methods called by the Options menu delegate.
  virtual bool IsTranslatableLanguageByPrefs();
  virtual void ToggleTranslatableLanguageByPrefs();
  virtual bool IsSiteBlacklisted();
  virtual void ToggleSiteBlacklist();
  virtual bool ShouldAlwaysTranslate();
  virtual void ToggleAlwaysTranslate();

  // Methods called by the extra-buttons that can appear on the "before
  // translate" infobar (when the user has accepted/declined the translation
  // several times).
  void AlwaysTranslatePageLanguage();
  void NeverTranslatePageLanguage();

  // The following methods are called by the infobar that displays the status
  // while translating and also the one displaying the error message.
  base::string16 GetMessageInfoBarText();
  base::string16 GetMessageInfoBarButtonText();
  void MessageInfoBarButtonPressed();
  bool ShouldShowMessageInfoBarButton();

  // Called by the before translate infobar to figure-out if it should show
  // an extra shortcut to let the user black-list/white-list that language
  // (based on how many times the user accepted/declined translation).
  // The shortcut itself is platform specific, it can be a button or a new bar
  // for example.
  bool ShouldShowNeverTranslateShortcut();
  bool ShouldShowAlwaysTranslateShortcut();

  // Convenience method that returns the displayable language name for
  // |language_code| in the current application locale.
  static base::string16 GetLanguageDisplayableName(
      const std::string& language_code);

  // Adds the strings that should be displayed in the after translate infobar to
  // |strings|. If |autodetermined_source_language| is false, the text in that
  // infobar is:
  // "The page has been translated from <lang1> to <lang2>."
  // Otherwise:
  // "The page has been translated to <lang1>."
  // Because <lang1>, or <lang1> and <lang2> are displayed in menu buttons, the
  // text is split in 2 or 3 chunks. |swap_languages| is set to true if
  // |autodetermined_source_language| is false, and <lang1> and <lang2>
  // should be inverted (some languages express the sentense as "The page has
  // been translate to <lang2> from <lang1>."). It is ignored if
  // |autodetermined_source_language| is true.
  static void GetAfterTranslateStrings(std::vector<base::string16>* strings,
                                       bool* swap_languages,
                                       bool autodetermined_source_language);

 protected:
  TranslateInfoBarDelegate(content::WebContents* web_contents,
                           TranslateTabHelper::TranslateStep step,
                           TranslateInfoBarDelegate* old_delegate,
                           const std::string& original_language,
                           const std::string& target_language,
                           TranslateErrors::Type error_type,
                           PrefService* prefs,
                           bool triggered_from_menu);

 private:
  friend class TranslationInfoBarTest;
  typedef std::pair<std::string, base::string16> LanguageNamePair;

  // Returns a translate infobar that owns |delegate|.
  static scoped_ptr<InfoBar> CreateInfoBar(
      scoped_ptr<TranslateInfoBarDelegate> delegate);

  // InfoBarDelegate:
  virtual void InfoBarDismissed() OVERRIDE;
  virtual int GetIconID() const OVERRIDE;
  virtual InfoBarDelegate::Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpire(const NavigationDetails& details) const OVERRIDE;
  virtual TranslateInfoBarDelegate* AsTranslateInfoBarDelegate() OVERRIDE;

  TranslateTabHelper::TranslateStep step_;

  // The type of fading animation if any that should be used when showing this
  // infobar.
  BackgroundAnimationType background_animation_;

  TranslateUIDelegate ui_delegate_;

  // The error that occurred when trying to translate (NONE if no error).
  TranslateErrors::Type error_type_;

  // The translation related preferences.
  scoped_ptr<TranslatePrefs> prefs_;

  // Whether the translation was triggered via a menu click vs automatically
  // (due to language detection, preferences...)
  bool triggered_from_menu_;
  DISALLOW_COPY_AND_ASSIGN(TranslateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_H_
