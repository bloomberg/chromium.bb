// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/hyphenator/hyphenator.h"

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/common/hyphenator_messages.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/hyphen/hyphen.h"
#include "third_party/icu/public/common/unicode/uscript.h"

namespace {

// A class that converts a sequence of UTF-8 characters to UTF-16 ones and holds
// only the length of converted UTF-16 characters. This class is used for
// creating a mapping from the position of a UTF-8 string to a position of a
// UTF-16 string without unnecessary conversions. Even though the following
// snippet produces the same mapping, it needs to convert same characters many
// times. This class incrementally counts the number of converted UTF-16
// characters to avoid this problem.
//
//   scoped_array<size_t> position(new size_t[text.length()]);
//   for (size_t i = 0; i < text.length(); ++i)
//     position[i] = UTF8ToUTF16(text.substr(0, i)).length();
//
class UTF16TextLength {
 public:
  UTF16TextLength();
  ~UTF16TextLength();

  // Returns the current position.
  int utf16_length() const { return utf16_length_; }

  // Appends one UTF-8 character to this converter and advances the converted
  // position. This converter increases the position by one when it finishes
  // reading a BMP character and increases by two when it finish reading a
  // non-BMP character.
  void Append(char c);

 private:
  // The length of the converted UTF-16 text.
  int utf16_length_;

  // The buffer that stores UTF-8 characters being converted.
  std::string utf8_text_;

  DISALLOW_COPY_AND_ASSIGN(UTF16TextLength);
};

UTF16TextLength::UTF16TextLength()
    : utf16_length_(0) {
}

UTF16TextLength::~UTF16TextLength() {
}

void UTF16TextLength::Append(char c) {
  // Append the given character and try converting the UTF-8 characters in this
  // buffer to Unicode codepoints. If this buffer includes a Unicode codepoint,
  // get the number of UTF-16 characters representing this codepoint and advance
  // the position.
  int code = 0;
  int index = 0;
  utf8_text_.push_back(c);
  U8_NEXT(utf8_text_.data(), index, static_cast<int>(utf8_text_.length()),
          code);
  if (code != U_SENTINEL) {
    utf8_text_.clear();
    utf16_length_ += U16_LENGTH(code);
  }
}

// A class that encapsulates a hyphenation query. This class owns resources
// temporarily needed for hyphenating one word, and deletes them when it is
// deleted as listed in the following snippet.
//
//   std::vector<int> hyphens;
//   QUery query(UTF8ToUTF16("hyphenate"));
//   query.Hyphenate(dict, &hyphens);
//
class Query {
 public:
  explicit Query(const string16& word);
  ~Query();

  // Hyphenates a word with the specified dictionary. This function hyphenates
  // the word provided to its constructor and returns a list of hyphenation
  // points, positions where we can insert hyphens.
  bool Hyphenate(HyphenDict* dictionary, std::vector<int>* hyphen_offsets);

 private:
  // A word to be hyphenated.
  std::string word_utf8_;

  // Return variables from the hyphen library.
  scoped_array<char> hyphen_vector_;
  char** rep_;
  int* pos_;
  int* cut_;

  DISALLOW_COPY_AND_ASSIGN(Query);
};

Query::Query(const string16& word)
    : rep_(NULL),
      pos_(NULL),
      cut_(NULL) {
  // Remove trailing punctuation characters. WebKit does not remove these
  // characters when it hyphenates a word. These characters prevent the hyphen
  // library from applying some rules, i.e. they prevent the library from adding
  // hyphens.
  DCHECK(!word.empty());
  const char16* data = word.data();
  int length = static_cast<int>(word.length());
  while (length > 0) {
    int previous = length;
    int code = 0;
    U16_PREV(data, 0, previous, code);
    UErrorCode error = U_ZERO_ERROR;
    if (uscript_getScript(code, &error) != USCRIPT_COMMON)
      break;
    length = previous;
  }
  UTF16ToUTF8(word.c_str(), length, &word_utf8_);
  // Create a hyphen vector used by hnj_hyphen_hyphenate2(). We allocate a
  // buffer of |word_.length()| + 5 as written in Line 112 of
  // <http://cs.chromium.org/src/third_party/hyphen/hyphen.h>.
  hyphen_vector_.reset(new char[word_utf8_.length() + 5]);
}

Query::~Query() {
  if (rep_) {
    for (size_t i = 0; i < word_utf8_.length(); ++i) {
      if (rep_[i])
        free(rep_[i]);
    }
    free(rep_);
  }
  if (pos_)
    free(pos_);
  if (cut_)
    free(cut_);
}

bool Query::Hyphenate(HyphenDict* dictionary,
                      std::vector<int>* hyphen_offsets) {
  DCHECK(dictionary);
  DCHECK(hyphen_offsets);

  int error_code = hnj_hyphen_hyphenate2(dictionary,
                                         word_utf8_.data(),
                                         static_cast<int>(word_utf8_.length()),
                                         hyphen_vector_.get(),
                                         NULL,
                                         &rep_,
                                         &pos_,
                                         &cut_);
  if (error_code)
    return false;

  // WebKit needs hyphenation points counted in UTF-16 characters. On the other
  // hand, the hyphen library returns hyphenation points counted in UTF-8
  // characters. We increamentally convert hyphenation points in UTF-8
  // characters to hyphenation points in UTF-16 characters and write the
  // converted hyphenation points to the output vector.
  UTF16TextLength text_length;
  hyphen_offsets->clear();
  for (size_t i = 0; i < word_utf8_.length(); ++i) {
    text_length.Append(word_utf8_[i]);
    if (hyphen_vector_[i] & 1)
      hyphen_offsets->push_back(text_length.utf16_length());
  }
  return !hyphen_offsets->empty();
}

}  // namespace

namespace content {

Hyphenator::Hyphenator(base::PlatformFile file)
    : dictionary_(NULL),
      dictionary_file_(base::FdopenPlatformFile(file, "r")),
      result_(0) {
}

Hyphenator::~Hyphenator() {
  if (dictionary_)
    hnj_hyphen_free(dictionary_);
}

bool Hyphenator::Initialize() {
  if (dictionary_)
    return true;

  if (!dictionary_file_.get())
    return false;
  dictionary_ = hnj_hyphen_load_file(dictionary_file_.get());
  return !!dictionary_;
}

bool Hyphenator::Attach(RenderThread* thread, const string16& locale) {
  if (!thread)
    return false;
  locale_.assign(locale);
  thread->AddObserver(this);
  return thread->Send(new HyphenatorHostMsg_OpenDictionary(locale));
}

bool Hyphenator::CanHyphenate(const string16& locale) {
  return !locale_.compare(locale);
}

size_t Hyphenator::ComputeLastHyphenLocation(const string16& word,
                                             size_t before_index) {
  if (!Initialize() || word.empty())
    return 0;

  // Call the hyphen library to get all hyphenation points, i.e. positions where
  // we can insert hyphens. When WebKit finds a line-break, it calls this
  // function twice or more with the same word to find the best hyphenation
  // point. To avoid calling the hyphen library twice or more with the same
  // word, we cache the last query.
  if (word_ != word) {
    word_ = word;
    Query query(word);
    result_ = query.Hyphenate(dictionary_, &hyphen_offsets_);
  }
  if (!result_)
    return 0;
  for (std::vector<int>::reverse_iterator it = hyphen_offsets_.rbegin();
       it != hyphen_offsets_.rend(); ++it) {
    if (static_cast<size_t>(*it) < before_index)
      return *it;
  }
  return 0;
}

bool Hyphenator::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(Hyphenator, message)
    IPC_MESSAGE_HANDLER(HyphenatorMsg_SetDictionary, OnSetDictionary)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void Hyphenator::OnSetDictionary(IPC::PlatformFileForTransit file) {
  base::PlatformFile rule_file =
      IPC::PlatformFileForTransitToPlatformFile(file);
  if (rule_file == base::kInvalidPlatformFileValue)
    return;
  // Delete the current dictionary and save the given file to this object. We
  // initialize the hyphen library the first time when WebKit actually
  // hyphenates a word, i.e. when WebKit calls the ComputeLastHyphenLocation
  // function. (WebKit does not always hyphenate words even when it calls the
  // CanHyphenate function, e.g. WebKit does not have to hyphenate words when it
  // does not have to break text into lines.)
  if (dictionary_) {
    hnj_hyphen_free(dictionary_);
    dictionary_ = NULL;
  }
  dictionary_file_.Set(base::FdopenPlatformFile(rule_file, "r"));
}

}  // namespace content
