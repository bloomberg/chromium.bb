// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_

#include <algorithm>
#include <bitset>
#include <map>
#include <string>

#include "base/bits.h"

class S2CellId;

// The node of a S2Cell-based quadtree holding string languages in its leaves.
class S2LangQuadTreeNode {
 public:
  S2LangQuadTreeNode();
  S2LangQuadTreeNode(const S2LangQuadTreeNode& other);
  ~S2LangQuadTreeNode();

  // Return language of the leaf containing the given |cell|.
  // Empty string if a null-leaf contains given |cell|.
  // |level_ptr| is set to the level (see S2CellId::level) of the leaf. (-1 if
  // |cell| matches an internal node).
  std::string Get(const S2CellId& cell, int* level_ptr) const;
  std::string Get(const S2CellId& cell) const {
    int level;
    return Get(cell, &level);
  }

  // Reconstruct a S2LangQuadTree with structure given by |tree| and with
  // languages given by |languages|. |tree| represents a depth-first traversal
  // of the tree with (1) internal nodes represented by a single bit 0, and (2)
  // leaf nodes represented by a bit 1 followed by the binary form of the index
  // of the language at that leaf into |languages|. We assume those indexes have
  // the smallest number of bits necessary. Indices are 1-based; index 0
  // represents absent language.
  // The bitset size is templated to allow this method to be re-used for the
  // small number of different-sized serialized trees we have.
  template <size_t numbits>
  static S2LangQuadTreeNode Deserialize(
      const std::vector<std::string>& languages,
      const std::bitset<numbits>& tree) {
    S2LangQuadTreeNode root;
    DeserializeSubtree(tree, 0, languages,
                       GetBitsPerLanguageIndex(languages.size()), &root);
    return root;
  }

 private:
  // Recursively deserialize the subtree at |bit_offset| in |serialized| into
  // the node |root|. |languages| and |bits_per_lang_index| dictacte
  // how languages in leaves are serialized.
  template <size_t numbits>
  static size_t DeserializeSubtree(const std::bitset<numbits>& serialized,
                                   size_t bit_offset,
                                   const std::vector<std::string>& languages,
                                   const int bits_per_lang_index,
                                   S2LangQuadTreeNode* root) {
    if (serialized[bit_offset]) {
      int index = 0;
      for (int bit = 1; bit <= bits_per_lang_index; bit++) {
        index <<= 1;
        index += serialized[bit_offset + bit];
      }
      if (index != 0)
        root->language_ = languages[index - 1];
      return bits_per_lang_index + 1;
    } else {
      size_t subtree_size = 1;
      for (int child_index = 0; child_index < 4; child_index++) {
        S2LangQuadTreeNode child;
        subtree_size +=
            DeserializeSubtree(serialized, bit_offset + subtree_size, languages,
                               bits_per_lang_index, &child);
        if (!child.IsNullLeaf())
          root->children_[child_index] = child;
      }
      return subtree_size;
    }
  }

  const S2LangQuadTreeNode* GetChild(const int child_index) const;

  // Return true iff the node is a leaf.
  bool IsLeaf() const;

  // Return true iff the node is a leaf with no language.
  bool IsNullLeaf() const;

  static int GetBitsPerLanguageIndex(const size_t num_languages) {
    DCHECK(num_languages > 0);
    return base::bits::Log2Ceiling(num_languages + 1);
  }

  std::map<int, S2LangQuadTreeNode> children_;
  std::string language_;
};

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_