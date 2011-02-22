// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the interface defined in spellchecker_platform_engine.h
// for the OS X platform.

#include "chrome/browser/spellchecker_platform_engine.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "base/time.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/browser_message_filter.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_common.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"

using base::TimeTicks;
namespace {
// The number of characters in the first part of the language code.
const unsigned int kShortLanguageCodeSize = 2;

// TextCheckingTask is reserved for spell checking against large size
// of text, which possible contains multiple paragrpahs.  Checking
// that size of text might take time, and should be done as a task on
// the FILE thread.
//
// The result of the check is returned back as a
// ViewMsg_SpellChecker_RespondTextCheck message.
class TextCheckingTask : public Task {
 public:
  TextCheckingTask(BrowserMessageFilter* destination,
                   int route_id,
                   int identifier,
                   const string16& text,
                   int document_tag)
      : destination_(destination),
        route_id_(route_id),
        identifier_(identifier),
        text_(text),
        document_tag_(document_tag) {
  }

  virtual void Run() {
    // TODO(morrita): Use [NSSpellChecker requestCheckingOfString]
    // when the build target goes up to 10.6
    std::vector<WebKit::WebTextCheckingResult> check_results;
    NSString* text_to_check = base::SysUTF16ToNSString(text_);
    size_t starting_at = 0;
    while (starting_at < text_.size()) {
      NSRange range = [[NSSpellChecker sharedSpellChecker]
                         checkSpellingOfString:text_to_check
                                    startingAt:starting_at
                                      language:nil
                                          wrap:NO
                        inSpellDocumentWithTag:document_tag_
                                     wordCount:NULL];
      if (range.length == 0)
        break;
      check_results.push_back(WebKit::WebTextCheckingResult(
          WebKit::WebTextCheckingResult::ErrorSpelling,
          range.location,
          range.length));
      starting_at = range.location + range.length;
    }

    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(
            destination_.get(),
            &BrowserMessageFilter::Send,
            new ViewMsg_SpellChecker_RespondTextCheck(route_id_,
                                                      identifier_,
                                                      document_tag_,
                                                      check_results)));
  }

 private:
  scoped_refptr<BrowserMessageFilter> destination_;
  int route_id_;
  int identifier_;
  string16 text_;
  int document_tag_;
};

// A private utility function to convert hunspell language codes to OS X
// language codes.
NSString* ConvertLanguageCodeToMac(const std::string& hunspell_lang_code) {
  NSString* whole_code = base::SysUTF8ToNSString(hunspell_lang_code);

  if ([whole_code length] > kShortLanguageCodeSize) {
    NSString* lang_code = [whole_code
                           substringToIndex:kShortLanguageCodeSize];
    // Add 1 here to skip the underscore.
    NSString* region_code = [whole_code
                             substringFromIndex:(kShortLanguageCodeSize + 1)];

    // Check for the special case of en-US and pt-PT, since OS X lists these
    // as just en and pt respectively.
    // TODO(pwicks): Find out if there are other special cases for languages
    // not installed on the system by default. Are there others like pt-PT?
    if (([lang_code isEqualToString:@"en"] &&
       [region_code isEqualToString:@"US"]) ||
        ([lang_code isEqualToString:@"pt"] &&
       [region_code isEqualToString:@"PT"])) {
      return lang_code;
    }

    // Otherwise, just build a string that uses an underscore instead of a
    // dash between the language and the region code, since this is the
    // format that OS X uses.
    NSString* os_x_language =
        [NSString stringWithFormat:@"%@_%@", lang_code, region_code];
    return os_x_language;
  } else {
    // Special case for Polish.
    if ([whole_code isEqualToString:@"pl"]) {
      return @"pl_PL";
    }
    // This is just a language code with the same format as OS X
    // language code.
    return whole_code;
  }
}

std::string ConvertLanguageCodeFromMac(NSString* lang_code) {
  // TODO(pwicks):figure out what to do about Multilingual
  // Guards for strange cases.
  if ([lang_code isEqualToString:@"en"]) return std::string("en-US");
  if ([lang_code isEqualToString:@"pt"]) return std::string("pt-PT");
  if ([lang_code isEqualToString:@"pl_PL"]) return std::string("pl");

  if ([lang_code length] > kShortLanguageCodeSize &&
      [lang_code characterAtIndex:kShortLanguageCodeSize] == '_') {
    return base::SysNSStringToUTF8([NSString stringWithFormat:@"%@-%@",
                [lang_code substringToIndex:kShortLanguageCodeSize],
                [lang_code substringFromIndex:(kShortLanguageCodeSize + 1)]]);
  }
  return base::SysNSStringToUTF8(lang_code);
}

} // namespace

namespace SpellCheckerPlatform {

void GetAvailableLanguages(std::vector<std::string>* spellcheck_languages) {
  NSArray* availableLanguages = [[NSSpellChecker sharedSpellChecker]
                        availableLanguages];
  for (NSString* lang_code in availableLanguages) {
    spellcheck_languages->push_back(
              ConvertLanguageCodeFromMac(lang_code));
  }
}

bool SpellCheckerAvailable() {
  // If this file was compiled, then we know that we are on OS X 10.5 at least
  // and can safely return true here.
  return true;
}

bool SpellCheckerProvidesPanel() {
  // OS X has a Spelling Panel, so we can return true here.
  return true;
}

bool SpellingPanelVisible() {
  // This should only be called from the main thread.
  DCHECK([NSThread currentThread] == [NSThread mainThread]);
  return [[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible];
}

void ShowSpellingPanel(bool show) {
  if (show) {
    [[[NSSpellChecker sharedSpellChecker] spellingPanel]
        performSelectorOnMainThread:@selector(makeKeyAndOrderFront:)
                         withObject:nil
                      waitUntilDone:YES];
  } else {
    [[[NSSpellChecker sharedSpellChecker] spellingPanel]
        performSelectorOnMainThread:@selector(close)
                         withObject:nil
                      waitUntilDone:YES];
  }
}

void UpdateSpellingPanelWithMisspelledWord(const string16& word) {
  NSString * word_to_display = base::SysUTF16ToNSString(word);
  [[NSSpellChecker sharedSpellChecker]
      performSelectorOnMainThread:
        @selector(updateSpellingPanelWithMisspelledWord:)
                       withObject:word_to_display
                    waitUntilDone:YES];
}

void Init() {
}

bool PlatformSupportsLanguage(const std::string& current_language) {
  // First, convert the language to an OS X language code.
  NSString* mac_lang_code = ConvertLanguageCodeToMac(current_language);

  // Then grab the languages available.
  NSArray* availableLanguages;
  availableLanguages = [[NSSpellChecker sharedSpellChecker]
                        availableLanguages];

  // Return true if the given language is supported by OS X.
  return [availableLanguages containsObject:mac_lang_code];
}

void SetLanguage(const std::string& lang_to_set) {
  NSString* NS_lang_to_set = ConvertLanguageCodeToMac(lang_to_set);
  [[NSSpellChecker sharedSpellChecker] setLanguage:NS_lang_to_set];
}

static int last_seen_tag_;

bool CheckSpelling(const string16& word_to_check, int tag) {
  last_seen_tag_ = tag;

  // [[NSSpellChecker sharedSpellChecker] checkSpellingOfString] returns an
  // NSRange that we can look at to determine if a word is misspelled.
  NSRange spell_range = {0,0};

  // Convert the word to an NSString.
  NSString* NS_word_to_check = base::SysUTF16ToNSString(word_to_check);
  // Check the spelling, starting at the beginning of the word.
  spell_range = [[NSSpellChecker sharedSpellChecker]
                  checkSpellingOfString:NS_word_to_check startingAt:0
                  language:nil wrap:NO inSpellDocumentWithTag:tag
                  wordCount:NULL];

  // If the length of the misspelled word == 0,
  // then there is no misspelled word.
  bool word_correct = (spell_range.length == 0);
  return word_correct;
}

void FillSuggestionList(const string16& wrong_word,
                        std::vector<string16>* optional_suggestions) {
  NSString* NS_wrong_word = base::SysUTF16ToNSString(wrong_word);
  TimeTicks begin_time = TimeTicks::Now();
  // The suggested words for |wrong_word|.
  NSArray* guesses =
      [[NSSpellChecker sharedSpellChecker] guessesForWord:NS_wrong_word];
  DHISTOGRAM_TIMES("Spellcheck.SuggestTime",
                   TimeTicks::Now() - begin_time);

  for (int i = 0; i < static_cast<int>([guesses count]); i++) {
    if (i < SpellCheckCommon::kMaxSuggestions) {
      optional_suggestions->push_back(base::SysNSStringToUTF16(
                                      [guesses objectAtIndex:i]));
    }
  }
}

void AddWord(const string16& word) {
    NSString* word_to_add = base::SysUTF16ToNSString(word);
  [[NSSpellChecker sharedSpellChecker] learnWord:word_to_add];
}

void RemoveWord(const string16& word) {
  NSString *word_to_remove = base::SysUTF16ToNSString(word);
  [[NSSpellChecker sharedSpellChecker] unlearnWord:word_to_remove];
}

int GetDocumentTag() {
  NSInteger doc_tag = [NSSpellChecker uniqueSpellDocumentTag];
  return static_cast<int>(doc_tag);
}

void IgnoreWord(const string16& word) {
  [[NSSpellChecker sharedSpellChecker] ignoreWord:base::SysUTF16ToNSString(word)
                           inSpellDocumentWithTag:last_seen_tag_];
}

void CloseDocumentWithTag(int tag) {
  [[NSSpellChecker sharedSpellChecker]
    closeSpellDocumentWithTag:static_cast<NSInteger>(tag)];
}

void RequestTextCheck(int route_id,
                      int identifier,
                      int document_tag,
                      const string16& text, BrowserMessageFilter* destination) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      new TextCheckingTask(
          destination, route_id, identifier, text, document_tag));
}

}  // namespace SpellCheckerPlatform
