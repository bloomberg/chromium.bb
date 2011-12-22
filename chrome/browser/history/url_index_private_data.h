// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
#define CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "chrome/browser/history/in_memory_url_index_types.h"

namespace history {

// A structure describing the index's internal data. This structure is used
// by the InMemoryURLIndex, InMemoryURLIndexBackend, and
// InMemoryURLCacheDatabase classes.
class URLIndexPrivateData {
 public:
  URLIndexPrivateData();
  ~URLIndexPrivateData();

  // Clear all of our data.
  void Clear();

  // Adds |word_id| to |history_id|'s entry in the history/word map,
  // creating a new entry if one does not already exist.
  void AddToHistoryIDWordMap(HistoryID history_id, WordID word_id);

  // Given a set of Char16s, finds words containing those characters.
  WordIDSet WordIDSetForTermChars(const Char16Set& term_chars);

 private:
  friend class InMemoryURLIndex;
  friend class InMemoryURLIndexTest;
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, CacheSaveRestore);
  FRIEND_TEST_ALL_PREFIXES(InMemoryURLIndexTest, TitleSearch);
  FRIEND_TEST_ALL_PREFIXES(LimitedInMemoryURLIndexTest, Initialization);

  // A list of all of indexed words. The index of a word in this list is the
  // ID of the word in the word_map_. It reduces the memory overhead by
  // replacing a potentially long and repeated string with a simple index.
  String16Vector word_list_;

  // A list of available words slots in |word_list_|. An available word slot
  // is the index of a unused word in word_list_ vector, also referred to as
  // a WordID. As URL visits are added or modified new words may be added to
  // the index, in which case any available words are used, if any, and then
  // words are added to the end of the word_list_. When URL visits are
  // modified or deleted old words may be removed from the index, in which
  // case the slots for those words are added to available_words_ for resuse
  // by future URL updates.
  WordIDSet available_words_;

  // A one-to-one mapping from the a word string to its slot number (i.e.
  // WordID) in the |word_list_|.
  WordMap word_map_;

  // A one-to-many mapping from a single character to all WordIDs of words
  // containing that character.
  CharWordIDMap char_word_map_;

  // A one-to-many mapping from a WordID to all HistoryIDs (the row_id as
  // used in the history database) of history items in which the word occurs.
  WordIDHistoryMap word_id_history_map_;

  // A one-to-many mapping from a HistoryID to all WordIDs of words that occur
  // in the URL and/or page title of the history item referenced by that
  // HistoryID.
  HistoryIDWordMap history_id_word_map_;

  // A one-to-one mapping from HistoryID to the history item data governing
  // index inclusion and relevance scoring.
  HistoryInfoMap history_info_map_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_URL_INDEX_PRIVATE_DATA_H_
