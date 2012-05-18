// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/spellchecker/spellcheck.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingCompletion.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCheckingResult.h"

using base::TimeTicks;
using content::RenderThread;
using WebKit::WebVector;
using WebKit::WebTextCheckingResult;
using WebKit::WebTextCheckingType;

namespace spellcheck {
void ToWebResultList(
    int offset,
    const std::vector<SpellCheckResult>& results,
    WebVector<WebTextCheckingResult>* web_results) {
  WebVector<WebTextCheckingResult> list(results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    list[i] = WebTextCheckingResult(
        static_cast<WebTextCheckingType>(results[i].type),
        results[i].location + offset,
        results[i].length,
        results[i].replacement);
  }

  list.swap(*web_results);
}

WebVector<WebTextCheckingResult> ToWebResultList(
    int offset,
    const std::vector<SpellCheckResult>& results) {
  WebVector<WebTextCheckingResult> web_results;
  ToWebResultList(offset, results, &web_results);
  return web_results;
}
} // namespace spellcheck

class SpellCheck::SpellCheckRequestParam
    : public base::RefCountedThreadSafe<SpellCheck::SpellCheckRequestParam> {
 public:
  SpellCheckRequestParam(const string16& text,
                         int offset,
                         WebKit::WebTextCheckingCompletion* completion)
      : text_(text),
        offset_(offset),
        completion_(completion) {
    DCHECK(completion);
  }

  string16 text() {
    return text_;
  }

  int offset() {
    return offset_;
  }

  WebKit::WebTextCheckingCompletion* completion() {
    return completion_;
  }

 private:
  friend class base::RefCountedThreadSafe<SpellCheckRequestParam>;

  ~SpellCheckRequestParam() {}

  // Text to be checked in this task.
  string16 text_;

  // The text offset from the beginning.
  int offset_;

  // The interface to send the misspelled ranges to WebKit.
  WebKit::WebTextCheckingCompletion* completion_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckRequestParam);
};

SpellCheck::SpellCheck()
    : file_(base::kInvalidPlatformFileValue),
      auto_spell_correct_turned_on_(false),
      is_using_platform_spelling_engine_(false),
      initialized_(false),
      dictionary_requested_(false) {
  // Wait till we check the first word before doing any initializing.
}

SpellCheck::~SpellCheck() {
}

bool SpellCheck::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpellCheck, message)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_WordAdded, OnWordAdded)
    IPC_MESSAGE_HANDLER(SpellCheckMsg_EnableAutoSpellCorrect,
                        OnEnableAutoSpellCorrect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SpellCheck::OnInit(IPC::PlatformFileForTransit bdict_file,
                        const std::vector<std::string>& custom_words,
                        const std::string& language,
                        bool auto_spell_correct) {
  Init(IPC::PlatformFileForTransitToPlatformFile(bdict_file),
       custom_words, language);
  auto_spell_correct_turned_on_ = auto_spell_correct;

  PostDelayedSpellCheckTask();
}

void SpellCheck::OnWordAdded(const std::string& word) {
  if (is_using_platform_spelling_engine_)
    return;

  if (!hunspell_.get()) {
    // Save it for later---add it when hunspell is initialized.
    custom_words_.push_back(word);
  } else {
    AddWordToHunspell(word);
  }
}

void SpellCheck::OnEnableAutoSpellCorrect(bool enable) {
  auto_spell_correct_turned_on_ = enable;
}

void SpellCheck::Init(base::PlatformFile file,
                      const std::vector<std::string>& custom_words,
                      const std::string& language) {
  initialized_ = true;
  hunspell_.reset();
  bdict_file_.reset();
  file_ = file;
  is_using_platform_spelling_engine_ =
      file == base::kInvalidPlatformFileValue && !language.empty();

  character_attributes_.SetDefaultLanguage(language);

  custom_words_.insert(custom_words_.end(),
                       custom_words.begin(), custom_words.end());

  // We delay the actual initialization of hunspell until it is needed.
}

bool SpellCheck::SpellCheckWord(
    const char16* in_word,
    int in_word_len,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<string16>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // Do nothing if we need to delay initialization. (Rather than blocking,
  // report the word as correctly spelled.)
  if (InitializeIfNeeded())
    return true;

  // Do nothing if spell checking is disabled.
  if (initialized_ && file_ == base::kInvalidPlatformFileValue &&
      !is_using_platform_spelling_engine_) {
    return true;
  }

  *misspelling_start = 0;
  *misspelling_len = 0;
  if (in_word_len == 0)
    return true;  // No input means always spelled correctly.

  string16 word;
  int word_start;
  int word_length;
  if (!text_iterator_.IsInitialized() &&
      !text_iterator_.Initialize(&character_attributes_, true)) {
      // We failed to initialize text_iterator_, return as spelled correctly.
      VLOG(1) << "Failed to initialize SpellcheckWordIterator";
      return true;
  }

  text_iterator_.SetText(in_word, in_word_len);
  while (text_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    if (CheckSpelling(word, tag))
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word, tag))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions)
      FillSuggestionList(word, optional_suggestions);
    return false;
  }

  return true;
}

bool SpellCheck::SpellCheckParagraph(
    const string16& text,
    std::vector<SpellCheckResult>* results) {
#if !defined(OS_MACOSX)
  // Mac has its own spell checker, so this method will not be used.
  DCHECK(results);

  size_t length = text.length();
  size_t offset = 0;

  // Spellcheck::SpellCheckWord() automatically breaks text into words and
  // checks the spellings of the extracted words. This function sets the
  // position and length of the first misspelled word and returns false when
  // the text includes misspelled words. Therefore, we just repeat calling the
  // function until it returns true to check the whole text.
  int misspelling_start = 0;
  int misspelling_length = 0;
  while (offset <= length) {
    if (SpellCheckWord(&text[offset],
                       length - offset,
                       0,
                       &misspelling_start,
                       &misspelling_length,
                       NULL)) {
      return true;
    }

    string16 replacement;
    results->push_back(SpellCheckResult(
        SpellCheckResult::SPELLING,
        misspelling_start + offset,
        misspelling_length,
        replacement));
    offset += misspelling_start + misspelling_length;
  }

  return false;
#else
  return true;
#endif
}

string16 SpellCheck::GetAutoCorrectionWord(const string16& word, int tag) {
  string16 autocorrect_word;
  if (!auto_spell_correct_turned_on_)
    return autocorrect_word;  // Return the empty string.

  int word_length = static_cast<int>(word.size());
  if (word_length < 2 || word_length > SpellCheckCommon::kMaxAutoCorrectWordSize)
    return autocorrect_word;

  if (InitializeIfNeeded())
    return autocorrect_word;

  char16 misspelled_word[SpellCheckCommon::kMaxAutoCorrectWordSize + 1];
  const char16* word_char = word.c_str();
  for (int i = 0; i <= SpellCheckCommon::kMaxAutoCorrectWordSize; i++) {
    if (i >= word_length)
      misspelled_word[i] = 0;
    else
      misspelled_word[i] = word_char[i];
  }

  // Swap adjacent characters and spellcheck.
  int misspelling_start, misspelling_len;
  for (int i = 0; i < word_length - 1; i++) {
    // Swap.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);

    // Check spelling.
    misspelling_start = misspelling_len = 0;
    SpellCheckWord(misspelled_word, word_length, tag, &misspelling_start,
        &misspelling_len, NULL);

    // Make decision: if only one swap produced a valid word, then we want to
    // return it. If we found two or more, we don't do autocorrection.
    if (misspelling_len == 0) {
      if (autocorrect_word.empty()) {
        autocorrect_word.assign(misspelled_word);
      } else {
        autocorrect_word.clear();
        break;
      }
    }

    // Restore the swapped characters.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);
  }
  return autocorrect_word;
}

void SpellCheck::RequestTextChecking(
    const string16& text,
    int offset,
    WebKit::WebTextCheckingCompletion* completion) {
#if !defined(OS_MACOSX)
  // Commented out on Mac, because SpellCheckRequest::PerformSpellCheck is not
  // implemented on Mac. Mac uses its own spellchecker, so this method
  // will not be used.
  DCHECK(!is_using_platform_spelling_engine_);

  // Clean up the previous request before starting a new request.
  if (pending_request_param_.get()) {
    pending_request_param_->completion()->didCancelCheckingText();
    pending_request_param_ = NULL;
  }

  if (InitializeIfNeeded()) {
    // We will check this text after we finish loading the hunspell dictionary.
    // Save parameters so that we can use them when we receive an init message
    // from the browser process.
    pending_request_param_ = new SpellCheckRequestParam(
        text, offset, completion);
    return;
  }

  requested_params_.push(new SpellCheckRequestParam(text, offset, completion));
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&SpellCheck::PerformSpellCheck, AsWeakPtr()));
#else
  NOTREACHED();
#endif
}

void SpellCheck::InitializeHunspell() {
  if (hunspell_.get())
    return;

  bdict_file_.reset(new file_util::MemoryMappedFile);

  if (bdict_file_->Initialize(file_)) {
    TimeTicks debug_start_time = base::Histogram::DebugNow();

    hunspell_.reset(
        new Hunspell(bdict_file_->data(), bdict_file_->length()));

    // Add custom words to Hunspell.
    for (std::vector<std::string>::iterator it = custom_words_.begin();
         it != custom_words_.end(); ++it) {
      AddWordToHunspell(*it);
    }

    DHISTOGRAM_TIMES("Spellcheck.InitTime",
                     base::Histogram::DebugNow() - debug_start_time);
  } else {
    NOTREACHED() << "Could not mmap spellchecker dictionary.";
  }
}

void SpellCheck::AddWordToHunspell(const std::string& word) {
  if (!word.empty() && word.length() < MAXWORDUTF8LEN)
    hunspell_->add(word.c_str());
}

bool SpellCheck::InitializeIfNeeded() {
  if (is_using_platform_spelling_engine_)
    return false;

  if (!initialized_ && !dictionary_requested_) {
    // RenderThread will not exist in test.
    if (RenderThread::Get())
      RenderThread::Get()->Send(new SpellCheckHostMsg_RequestDictionary);
    dictionary_requested_ = true;
    return true;
  }

  // Don't initialize if hunspell is disabled.
  if (file_ != base::kInvalidPlatformFileValue)
    InitializeHunspell();

  return !initialized_;
}

// When called, relays the request to check the spelling to the proper
// backend, either hunspell or a platform-specific backend.
bool SpellCheck::CheckSpelling(const string16& word_to_check, int tag) {
  bool word_correct = false;

  if (is_using_platform_spelling_engine_) {
#if defined(OS_MACOSX)
    RenderThread::Get()->Send(new SpellCheckHostMsg_CheckSpelling(
        word_to_check, tag, &word_correct));
#endif
  } else {
    std::string word_to_check_utf8(UTF16ToUTF8(word_to_check));
    // Hunspell shouldn't let us exceed its max, but check just in case
    if (word_to_check_utf8.length() < MAXWORDUTF8LEN) {
      if (hunspell_.get()) {
        // |hunspell_->spell| returns 0 if the word is spelled correctly and
        // non-zero otherwsie.
        word_correct = (hunspell_->spell(word_to_check_utf8.c_str()) != 0);
      } else {
        // If |hunspell_| is NULL here, an error has occurred, but it's better
        // to check rather than crash.
        word_correct = true;
      }
    }
  }

  return word_correct;
}

void SpellCheck::PostDelayedSpellCheckTask() {
  if (!pending_request_param_)
    return;

  if (file_ == base::kInvalidPlatformFileValue) {
    pending_request_param_->completion()->didCancelCheckingText();
  } else {
    requested_params_.push(pending_request_param_);
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&SpellCheck::PerformSpellCheck, AsWeakPtr()));
  }

  pending_request_param_ = NULL;
}

void SpellCheck::PerformSpellCheck() {
#if !defined(OS_MACOSX)
  DCHECK(!requested_params_.empty());
  scoped_refptr<SpellCheckRequestParam> param = requested_params_.front();
  DCHECK(param);
  requested_params_.pop();

  std::vector<SpellCheckResult> results;
  SpellCheckParagraph(param->text(), &results);
  param->completion()->didFinishCheckingText(
      spellcheck::ToWebResultList(param->offset(), results));
#else
  // SpellCheck::SpellCheckParagraph is not implemented on Mac,
  // so we return without spellchecking. Note that Mac uses its own
  // spellchecker, this function won't be used.
  NOTREACHED();
#endif
}

void SpellCheck::FillSuggestionList(
    const string16& wrong_word,
    std::vector<string16>* optional_suggestions) {
  if (is_using_platform_spelling_engine_) {
#if defined(OS_MACOSX)
    RenderThread::Get()->Send(new SpellCheckHostMsg_FillSuggestionList(
        wrong_word, optional_suggestions));
#endif
    return;
  }

  // If |hunspell_| is NULL here, an error has occurred, but it's better
  // to check rather than crash.
  if (!hunspell_.get())
    return;

  char** suggestions;
  int number_of_suggestions =
      hunspell_->suggest(&suggestions, UTF16ToUTF8(wrong_word).c_str());

  // Populate the vector of WideStrings.
  for (int i = 0; i < number_of_suggestions; i++) {
    if (i < SpellCheckCommon::kMaxSuggestions)
      optional_suggestions->push_back(UTF8ToUTF16(suggestions[i]));
    free(suggestions[i]);
  }
  if (suggestions != NULL)
    free(suggestions);
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellCheck::IsValidContraction(const string16& contraction, int tag) {
  if (!contraction_iterator_.IsInitialized() &&
      !contraction_iterator_.Initialize(&character_attributes_, false)) {
    // We failed to initialize the word iterator, return as spelled correctly.
    VLOG(1) << "Failed to initialize contraction_iterator_";
    return true;
  }

  contraction_iterator_.SetText(contraction.c_str(), contraction.length());

  string16 word;
  int word_start;
  int word_length;
  while (contraction_iterator_.GetNextWord(&word, &word_start, &word_length)) {
    if (!CheckSpelling(word, tag))
      return false;
  }
  return true;
}
