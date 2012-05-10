// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"

#include <list>
#include <map>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/ubrk.h"

namespace safe_browsing {

// This time should be short enough that it doesn't noticeably disrupt the
// user's interaction with the page.
const int PhishingTermFeatureExtractor::kMaxTimePerChunkMs = 10;

// Experimenting shows that we get a reasonable gain in performance by
// increasing this up to around 10, but there's not much benefit in
// increasing it past that.
const int PhishingTermFeatureExtractor::kClockCheckGranularity = 5;

// This should be longer than we expect feature extraction to take on any
// actual phishing page.
const int PhishingTermFeatureExtractor::kMaxTotalTimeMs = 500;

// The maximum size of the negative word cache.
const int PhishingTermFeatureExtractor::kMaxNegativeWordCacheSize = 1000;

// All of the state pertaining to the current feature extraction.
struct PhishingTermFeatureExtractor::ExtractionState {
  // Stores up to max_words_per_term_ previous words separated by spaces.
  std::string previous_words;

  // Stores the sizes of the words in previous_words.  Note: the size includes
  // the space after each word.  In other words, the sum of all sizes in this
  // list is equal to the length of previous_words.
  std::list<size_t> previous_word_sizes;

  // An iterator for word breaking.
  UBreakIterator* iterator;

  // Our current position in the text that was passed to the ExtractionState
  // constructor, speciailly, the most recent break position returned by our
  // iterator.
  int position;

  // True if position has been initialized.
  bool position_initialized;

  // The time at which we started feature extraction for the current page.
  base::TimeTicks start_time;

  // The number of iterations we've done for the current extraction.
  int num_iterations;

  ExtractionState(const string16& text, base::TimeTicks start_time_ticks)
      : position(-1),
        position_initialized(false),
        start_time(start_time_ticks),
        num_iterations(0) {
    UErrorCode status = U_ZERO_ERROR;
    // TODO(bryner): We should pass in the language for the document.
    iterator = ubrk_open(UBRK_WORD, NULL,
                         text.data(), text.size(),
                         &status);
    if (U_FAILURE(status)) {
      DLOG(ERROR) << "ubrk_open failed: " << status;
      iterator = NULL;
    }
  }

  ~ExtractionState() {
    if (iterator) {
      ubrk_close(iterator);
    }
  }
};

PhishingTermFeatureExtractor::PhishingTermFeatureExtractor(
    const base::hash_set<std::string>* page_term_hashes,
    const base::hash_set<uint32>* page_word_hashes,
    size_t max_words_per_term,
    uint32 murmurhash3_seed,
    FeatureExtractorClock* clock)
    : page_term_hashes_(page_term_hashes),
      page_word_hashes_(page_word_hashes),
      max_words_per_term_(max_words_per_term),
      murmurhash3_seed_(murmurhash3_seed),
      negative_word_cache_(kMaxNegativeWordCacheSize),
      clock_(clock),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  Clear();
}

PhishingTermFeatureExtractor::~PhishingTermFeatureExtractor() {
  // The RenderView should have called CancelPendingExtraction() before
  // we are destroyed.
  CheckNoPendingExtraction();
}

void PhishingTermFeatureExtractor::ExtractFeatures(
    const string16* page_text,
    FeatureMap* features,
    const DoneCallback& done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  CheckNoPendingExtraction();
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  page_text_ = page_text;
  features_ = features;
  done_callback_ = done_callback;

  state_.reset(new ExtractionState(*page_text_, clock_->Now()));
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout,
                 weak_factory_.GetWeakPtr()));
}

void PhishingTermFeatureExtractor::CancelPendingExtraction() {
  // Cancel any pending callbacks, and clear our state.
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout() {
  DCHECK(state_.get());
  ++state_->num_iterations;
  base::TimeTicks current_chunk_start_time = clock_->Now();

  if (!state_->iterator) {
    // We failed to initialize the break iterator, so stop now.
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.TermFeatureBreakIterError", 1);
    RunCallback(false);
    return;
  }

  if (!state_->position_initialized) {
    state_->position = ubrk_first(state_->iterator);
    if (state_->position == UBRK_DONE) {
      // No words present, so we're done.
      RunCallback(true);
      return;
    }
    state_->position_initialized = true;
  }

  int num_words = 0;
  for (int next = ubrk_next(state_->iterator);
       next != UBRK_DONE; next = ubrk_next(state_->iterator)) {
    if (ubrk_getRuleStatus(state_->iterator) != UBRK_WORD_NONE) {
      // next is now positioned at the end of a word.
      HandleWord(base::StringPiece16(page_text_->data() + state_->position,
                                     next - state_->position));
      ++num_words;
    }
    state_->position = next;

    if (num_words >= kClockCheckGranularity) {
      num_words = 0;
      base::TimeTicks now = clock_->Now();
      if (now - state_->start_time >=
          base::TimeDelta::FromMilliseconds(kMaxTotalTimeMs)) {
        DLOG(ERROR) << "Feature extraction took too long, giving up";
        // We expect this to happen infrequently, so record when it does.
        UMA_HISTOGRAM_COUNTS("SBClientPhishing.TermFeatureTimeout", 1);
        RunCallback(false);
        return;
      }
      base::TimeDelta chunk_elapsed = now - current_chunk_start_time;
      if (chunk_elapsed >=
          base::TimeDelta::FromMilliseconds(kMaxTimePerChunkMs)) {
        // The time limit for the current chunk is up, so post a task to
        // continue extraction.
        //
        // Record how much time we actually spent on the chunk.  If this is
        // much higher than kMaxTimePerChunkMs, we may need to adjust the
        // clock granularity.
        UMA_HISTOGRAM_TIMES("SBClientPhishing.TermFeatureChunkTime",
                            chunk_elapsed);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(
                &PhishingTermFeatureExtractor::ExtractFeaturesWithTimeout,
                weak_factory_.GetWeakPtr()));
        return;
      }
      // Otherwise, continue.
    }
  }
  RunCallback(true);
}

void PhishingTermFeatureExtractor::HandleWord(
    const base::StringPiece16& word) {
  // Quickest out if we have seen this word before and know that it's not
  // part of any term. This avoids the lowercasing and UTF conversion, both of
  // which are relatively expensive.
  if (negative_word_cache_.Get(word) != negative_word_cache_.end()) {
    // We know we're no longer in a possible n-gram, so clear the previous word
    // state.
    state_->previous_words.clear();
    state_->previous_word_sizes.clear();
    return;
  }

  std::string word_lower = UTF16ToUTF8(base::i18n::ToLower(word));
  uint32 word_hash = MurmurHash3String(word_lower, murmurhash3_seed_);

  // Quick out if the word is not part of any term, which is the common case.
  if (page_word_hashes_->find(word_hash) == page_word_hashes_->end()) {
    // Word doesn't exist in our terms so we can clear the n-gram state.
    state_->previous_words.clear();
    state_->previous_word_sizes.clear();
    // Insert into negative cache so that we don't try this again.
    negative_word_cache_.Put(word, true);
    return;
  }

  // Find all of the n-grams that we need to check and compute their SHA-256
  // hashes.
  std::map<std::string /* hash */, std::string /* plaintext */>
      hashes_to_check;
  hashes_to_check[crypto::SHA256HashString(word_lower)] = word_lower;

  // Combine the new word with the previous words to find additional n-grams.
  // Note that we don't yet add the new word length to previous_word_sizes,
  // since we don't want to compute the hash for the word by itself again.
  //
  state_->previous_words.append(word_lower);
  std::string current_term = state_->previous_words;
  for (std::list<size_t>::iterator it = state_->previous_word_sizes.begin();
       it != state_->previous_word_sizes.end(); ++it) {
    hashes_to_check[crypto::SHA256HashString(current_term)] = current_term;
    current_term.erase(0, *it);
  }

  // Add features for any hashes that match page_term_hashes_.
  for (std::map<std::string, std::string>::iterator it =
           hashes_to_check.begin();
       it != hashes_to_check.end(); ++it) {
    if (page_term_hashes_->find(it->first) != page_term_hashes_->end()) {
      features_->AddBooleanFeature(features::kPageTerm + it->second);
    }
  }

  // Now that we have handled the current word, we have to add a space at the
  // end of it, and add the new word's size (including the space) to
  // previous_word_sizes.  Note: it's possible that the document language
  // doesn't use ASCII spaces to separate words.  That's fine though, we just
  // need to be consistent with how the model is generated.
  state_->previous_words.append(" ");
  state_->previous_word_sizes.push_back(word_lower.size() + 1);

  // Cap the number of previous words.
  if (state_->previous_word_sizes.size() >= max_words_per_term_) {
    state_->previous_words.erase(0, state_->previous_word_sizes.front());
    state_->previous_word_sizes.pop_front();
  }
}

void PhishingTermFeatureExtractor::CheckNoPendingExtraction() {
  DCHECK(done_callback_.is_null());
  DCHECK(!state_.get());
  if (!done_callback_.is_null() || state_.get()) {
    LOG(ERROR) << "Extraction in progress, missing call to "
               << "CancelPendingExtraction";
  }
}

void PhishingTermFeatureExtractor::RunCallback(bool success) {
  // Record some timing stats that we can use to evaluate feature extraction
  // performance.  These include both successful and failed extractions.
  DCHECK(state_.get());
  UMA_HISTOGRAM_COUNTS("SBClientPhishing.TermFeatureIterations",
                       state_->num_iterations);
  UMA_HISTOGRAM_TIMES("SBClientPhishing.TermFeatureTotalTime",
                      clock_->Now() - state_->start_time);

  DCHECK(!done_callback_.is_null());
  done_callback_.Run(success);
  Clear();
}

void PhishingTermFeatureExtractor::Clear() {
  page_text_ = NULL;
  features_ = NULL;
  done_callback_.Reset();
  state_.reset(NULL);
  negative_word_cache_.Clear();
}

}  // namespace safe_browsing
