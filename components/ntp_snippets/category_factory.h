// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/ntp_snippets/category.h"

namespace ntp_snippets {

// Creates and orders Category instances.
class CategoryFactory {
 public:
  CategoryFactory();
  ~CategoryFactory();

  // Creates a category from a KnownCategory value. The passed |known_category|
  // must not be one of the special values (LOCAL_CATEGORIES_COUNT or
  // REMOTE_CATEGORIES_OFFSET).
  Category FromKnownCategory(KnownCategories known_category);

  // Creates a category from a category identifier delivered by the server.
  // |remote_category| must be positive.
  // Note that remote categories are ordered in the order in which they were
  // first created by calling this method.
  Category FromRemoteCategory(int remote_category);

  // Creates a category from an ID as returned by |Category::id()|.
  // |id| must be a non-negative value.
  Category FromIDValue(int id);

  // Compares the given categories according to a strict ordering, returning
  // true if and only if |left| is strictly less than |right|.
  // This method satisfies the "Compare" contract required by sort algorithms.
  // The order is determined as follows: All local categories go first, in a
  // specific order hard-coded in the |CategoryFactory| constructor. All remote
  // categories follow in the order in which they were first created through
  // |FromRemoteCategory|.
  bool CompareCategories(const Category& left, const Category& right) const;

  // TODO(treib): Remove the following 3 functions from here once we move to a
  // more structured identification than the unique_id string and thus once we
  // eliminate these functions. See crbug.com/649048.

  // Creates a unique ID. The given |within_category_id| must be unique among
  // all suggestion IDs from this provider for the given |category|. This method
  // combines it with the |category| to form an ID that is unique
  // application-wide, because this provider is the only one that provides
  // suggestions for that category.
  std::string MakeUniqueID(Category category,
                           const std::string& within_category_id) const;

  // Reverse functions for MakeUniqueID()
  Category GetCategoryFromUniqueID(const std::string& unique_id);
  std::string GetWithinCategoryIDFromUniqueID(
      const std::string& unique_id) const;

 private:
  bool CategoryExists(int id);
  void AddKnownCategory(KnownCategories known_category);
  Category InternalFromID(int id);

  // Stores all known categories in the order which is also returned by
  // |CompareCategories|.
  std::vector<Category> ordered_categories_;

  DISALLOW_COPY_AND_ASSIGN(CategoryFactory);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_FACTORY_H_
