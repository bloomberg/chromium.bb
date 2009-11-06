// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_BOOKMARK_MODEL_VERIFIER_H_
#define CHROME_TEST_LIVE_SYNC_BOOKMARK_MODEL_VERIFIER_H_

#include <string>
#include "chrome/browser/profile.h"
#include "googleurl/src/gurl.h"

class BookmarkModel;
class BookmarkNode;

// A tool to perform bookmark model operations on a model and have
// the changes echoed in a "verifier" model that can be used as an expected
// hierarchy to compare against.
// Note when we refer to the "same" nodes in |model| and |verifier| parameters,
// we mean same the canonical bookmark entity, because |verifier| is expected
// to be a replica of |model|.
class BookmarkModelVerifier {
 public:
  ~BookmarkModelVerifier() { }

  // Creates a BookmarkModelVerifier and waits until the model has loaded.
  // Caller takes ownership.
  static BookmarkModelVerifier* Create();

  // Adds the same folder to |model| and |verifier|.
  // See BookmarkModel::AddGroup for details.
  const BookmarkNode* AddGroup(BookmarkModel* model,
                               const BookmarkNode* parent,
                               int index,
                               const std::wstring& title);

  // Adds the same non-empty folder to |model| and |verifier|.
  // It also adds specified number of childern (mix of bm and folder).
  const BookmarkNode* AddNonEmptyGroup(BookmarkModel* model,
                               const BookmarkNode* parent,
                               int index,
                               const std::wstring& title,
                               int children_count);

  // Adds the same bookmark to |model| and |verifier|.
  // See BookmarkModel::AddURL for details.
  const BookmarkNode* AddURL(BookmarkModel* model,
                             const BookmarkNode* parent,
                             int index,
                             const std::wstring& title,
                             const GURL& url);

  // Sets the title of the same node in |model| and |verifier|.
  // See BookmarkModel::SetTitle for details.
  void SetTitle(BookmarkModel* model, const BookmarkNode* node,
                const std::wstring& title);

  // Moves the same node to the same position in both |model| and |verifier|.
  // See BookmarkModel::Move for details.
  void Move(BookmarkModel* model,
            const BookmarkNode* node,
            const BookmarkNode* new_parent,
            int index);

  // Removes the same node in |model| and |verifier|.
  // See BookmarkModel::Remove for details.
  void Remove(BookmarkModel* model, const BookmarkNode* parent, int index);

  // Sorts children of the same parent node in |model| and |verifier|.
  // See BookmarkModel::SortChildren for details.
  void SortChildren(BookmarkModel* model, const BookmarkNode* parent);

  // Reverses the order of children of the same parent node in |model|
  // and |verifier|.
  void ReverseChildOrder(BookmarkModel* model, const BookmarkNode* parent);

  const BookmarkNode* SetURL(BookmarkModel* model,
              const BookmarkNode* node,
              const GURL& new_url);

  // Asserts that the verifier model and |actual| are equivalent.
  void ExpectMatch(BookmarkModel* actual);

  // Asserts that the two hierarchies are equivalent in terms of the data model.
  // (e.g some peripheral fields like creation times are allowed to mismatch).
  static void ExpectModelsMatch(BookmarkModel* expected,
                                BookmarkModel* actual) {
    ExpectModelsMatchIncludingFavicon(expected, actual, false);
  }

  static void ExpectModelsMatchIncludingFavicon(BookmarkModel* expected,
                                BookmarkModel* actual,
                                bool compare_favicon);

  static void VerifyNoDuplicates(BookmarkModel* model);

 private:
  BookmarkModelVerifier();
  void FindNodeInVerifier(BookmarkModel* foreign_model,
                          const BookmarkNode* foreign_node,
                          const BookmarkNode** result);

  // Does a deep comparison of BookmarkNode fields between the two parameters,
  // and asserts equality.
  static void ExpectBookmarkInfoMatch(const BookmarkNode* expected,
                                      const BookmarkNode* actual);

  scoped_ptr<Profile> verifier_profile_;
  BookmarkModel* verifier_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkModelVerifier);
};

#endif  // CHROME_TEST_LIVE_SYNC_BOOKMARK_MODEL_VERIFIER_H_
