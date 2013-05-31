// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_
#define CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_

#include <map>
#include <set>
#include <vector>

#include "chrome/browser/spellchecker/misspelling.h"

namespace spellcheck {

// Stores feedback for the spelling service in |Misspelling| objects. Each
// |Misspelling| object is identified by a |hash| and corresponds to a document
// marker  with the same |hash| identifier in the renderer.
class Feedback {
 public:
  Feedback();
  ~Feedback();

  // Returns the misspelling identified by |hash|. Returns NULL if there's no
  // misspelling identified by |hash|. Retains the ownership of the result. The
  // caller should not modify the hash in the returned misspelling.
  Misspelling* GetMisspelling(uint32 hash);

  // Finalizes the user actions on misspellings that are removed from the
  // renderer process with ID |renderer_process_id|.
  void FinalizeRemovedMisspellings(
      int renderer_process_id,
      const std::vector<uint32>& remaining_markers);

  // Returns true if the renderer with process ID |renderer_process_id| has
  // misspellings. Otherwise returns false.
  bool RendererHasMisspellings(int renderer_process_id) const;

  // Returns a copy of the misspellings in renderer with process ID
  // |renderer_process_id|.
  std::vector<Misspelling> GetMisspellingsInRenderer(
      int renderer_process_id) const;

  // Erases the misspellings with final user actions in the renderer with
  // process ID |renderer_process_id|.
  void EraseFinalizedMisspellings(int renderer_process_id);

  // Returns true if there's a misspelling with |hash| identifier. Otherwise
  // returns false.
  bool HasMisspelling(uint32 hash) const;

  // Adds the |misspelling| to feedback data. If the |misspelling| has a
  // duplicate hash, then replaces the existing misspelling with the same hash.
  void AddMisspelling(int renderer_process_id, const Misspelling& misspelling);

  // Returns true if there're no misspellings. Otherwise returns false.
  bool Empty() const;

  // Returns a list of process identifiers for renderers that have misspellings.
  std::vector<int> GetRendersWithMisspellings() const;

  // Finalizes all misspellings.
  void FinalizeAllMisspellings();

  // Returns a copy of all misspellings.
  std::vector<Misspelling> GetAllMisspellings() const;

  // Removes all misspellings.
  void Clear();

  // Returns a list of all misspelling identifiers for |misspelled_text|.
  const std::set<uint32>& FindMisspellings(const string16& misspelled_text);

 private:
  // A map of hashes that identify document markers to feedback data to be sent
  // to spelling service.
  typedef std::map<uint32, Misspelling> HashMisspellingMap;
  HashMisspellingMap misspellings_;

  // A map of renderer process ID to hashes that identify misspellings.
  typedef std::set<uint32> HashCollection;
  typedef std::map<int, HashCollection> RendererHashesMap;
  RendererHashesMap renderers_;

  // A map of misspelled text to hashes that identify misspellings.
  typedef std::map<string16, HashCollection> TextHashesMap;
  TextHashesMap text_;

  DISALLOW_COPY_AND_ASSIGN(Feedback);
};

}  // namespace spellcheck

#endif  // CHROME_BROWSER_SPELLCHECKER_FEEDBACK_H_
