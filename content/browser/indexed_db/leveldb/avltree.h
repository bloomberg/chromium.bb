// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Based on Abstract AVL Tree Template v1.5 by Walt Karas
 * <http://geocities.com/wkaras/gen_cpp/avl_tree.html>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_AVLTREE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_AVLTREE_H_

#include "base/logging.h"
#include "content/browser/indexed_db/leveldb/fixed_array.h"

namespace content {

// Here is the reference class for BSet.
//
// class BSet
//   {
//   public:
//
//     class ANY_bitref
//       {
//       public:
//         operator bool ();
//         void operator = (bool b);
//       };
//
//     // Does not have to initialize bits.
//     BSet();
//
//     // Must return a valid value for index when 0 <= index < maxDepth
//     ANY_bitref operator [] (unsigned index);
//
//     // Set all bits to 1.
//     void Set();
//
//     // Set all bits to 0.
//     void Reset();
//   };

template <unsigned maxDepth>
class AVLTreeDefaultBSet {
 public:
  bool& operator[](unsigned i) {
#if defined(ADDRESS_SANITIZER)
    CHECK(i < maxDepth);
#endif
    return data_[i];
  }
  void Set() {
    for (unsigned i = 0; i < maxDepth; ++i)
      data_[i] = true;
  }
  void Reset() {
    for (unsigned i = 0; i < maxDepth; ++i)
      data_[i] = false;
  }

 private:
  FixedArray<bool, maxDepth> data_;
};

// How to determine maxDepth:
// d  Minimum number of nodes
// 2  2
// 3  4
// 4  7
// 5  12
// 6  20
// 7  33
// 8  54
// 9  88
// 10 143
// 11 232
// 12 376
// 13 609
// 14 986
// 15 1,596
// 16 2,583
// 17 4,180
// 18 6,764
// 19 10,945
// 20 17,710
// 21 28,656
// 22 46,367
// 23 75,024
// 24 121,392
// 25 196,417
// 26 317,810
// 27 514,228
// 28 832,039
// 29 1,346,268
// 30 2,178,308
// 31 3,524,577
// 32 5,702,886
// 33 9,227,464
// 34 14,930,351
// 35 24,157,816
// 36 39,088,168
// 37 63,245,985
// 38 102,334,154
// 39 165,580,140
// 40 267,914,295
// 41 433,494,436
// 42 701,408,732
// 43 1,134,903,169
// 44 1,836,311,902
// 45 2,971,215,072
//
// E.g., if, in a particular instantiation, the maximum number of nodes in a
// tree instance is 1,000,000, the maximum depth should be 28.
// You pick 28 because MN(28) is 832,039, which is less than or equal to
// 1,000,000, and MN(29) is 1,346,268, which is strictly greater than 1,000,000.

template <class Abstractor,
          unsigned maxDepth = 32,
          class BSet = AVLTreeDefaultBSet<maxDepth> >
class AVLTree {
 public:
  typedef typename Abstractor::key key;
  typedef typename Abstractor::handle handle;
  typedef typename Abstractor::size size;

  enum SearchType {
    EQUAL = 1,
    LESS = 2,
    GREATER = 4,
    LESS_EQUAL = EQUAL | LESS,
    GREATER_EQUAL = EQUAL | GREATER
  };

  Abstractor& abstractor() { return abs_; }

  inline handle Insert(handle h);

  inline handle Search(key k, SearchType st = EQUAL);
  inline handle SearchLeast();
  inline handle SearchGreatest();

  inline handle Remove(key k);

  inline handle Subst(handle new_node);

  void Purge() { abs_.root = Null(); }

  bool IsEmpty() { return abs_.root == Null(); }

  AVLTree() { abs_.root = Null(); }

  class Iterator {
   public:
    // Initialize depth to invalid value, to indicate iterator is
    // invalid.   (Depth is zero-base.)
    Iterator() { depth_ = ~0U; }

    void StartIter(AVLTree* tree, key k, SearchType st = EQUAL) {
      // Mask of high bit in an int.
      const int kMaskHighBit = static_cast<int>(~((~(unsigned)0) >> 1));

      // Save the tree that we're going to iterate through in a
      // member variable.
      tree_ = tree;

      int cmp, target_cmp;
      handle h = tree_->abs_.root;
      unsigned d = 0;

      depth_ = ~0U;

      if (h == Null()) {
        // Tree is empty.
        return;
      }

      if (st & LESS) {
        // Key can be greater than key of starting node.
        target_cmp = 1;
      } else if (st & GREATER) {
        // Key can be less than key of starting node.
        target_cmp = -1;
      } else {
        // Key must be same as key of starting node.
        target_cmp = 0;
      }

      for (;;) {
        cmp = CmpKN(k, h);
        if (cmp == 0) {
          if (st & EQUAL) {
            // Equal node was sought and found as starting node.
            depth_ = d;
            break;
          }
          cmp = -target_cmp;
        } else if (target_cmp != 0) {
          if (!((cmp ^ target_cmp) & kMaskHighBit)) {
            // cmp and target_cmp are both negative or both positive.
            depth_ = d;
          }
        }
        h = cmp < 0 ? GetLT(h) : GetGT(h);
        if (h == Null())
          break;
        branch_[d] = cmp > 0;
        path_h_[d++] = h;
      }
    }

    void StartIterLeast(AVLTree* tree) {
      tree_ = tree;

      handle h = tree_->abs_.root;

      depth_ = ~0U;

      branch_.Reset();

      while (h != Null()) {
        if (depth_ != ~0U)
          path_h_[depth_] = h;
        depth_++;
        h = GetLT(h);
      }
    }

    void StartIterGreatest(AVLTree* tree) {
      tree_ = tree;

      handle h = tree_->abs_.root;

      depth_ = ~0U;

      branch_.Set();

      while (h != Null()) {
        if (depth_ != ~0U)
          path_h_[depth_] = h;
        depth_++;
        h = GetGT(h);
      }
    }

    handle operator*() {
      if (depth_ == ~0U)
        return Null();

      return depth_ == 0 ? tree_->abs_.root : path_h_[depth_ - 1];
    }

    void operator++() {
      if (depth_ != ~0U) {
        handle h = GetGT(**this);
        if (h == Null()) {
          do {
            if (depth_ == 0) {
              depth_ = ~0U;
              break;
            }
            depth_--;
          } while (branch_[depth_]);
        } else {
          branch_[depth_] = true;
          path_h_[depth_++] = h;
          for (;;) {
            h = GetLT(h);
            if (h == Null())
              break;
            branch_[depth_] = false;
            path_h_[depth_++] = h;
          }
        }
      }
    }

    void operator--() {
      if (depth_ != ~0U) {
        handle h = GetLT(**this);
        if (h == Null()) {
          do {
            if (depth_ == 0) {
              depth_ = ~0U;
              break;
            }
            depth_--;
          } while (!branch_[depth_]);
        } else {
          branch_[depth_] = false;
          path_h_[depth_++] = h;
          for (;;) {
            h = GetGT(h);
            if (h == Null())
              break;
            branch_[depth_] = true;
            path_h_[depth_++] = h;
          }
        }
      }
    }

    void operator++(int /*ignored*/) { ++(*this); }
    void operator--(int /*ignored*/) { --(*this); }

   protected:
    // Tree being iterated over.
    AVLTree* tree_;

    // Records a path into the tree.  If branch_[n] is true, indicates
    // take greater branch from the nth node in the path, otherwise
    // take the less branch.  branch_[0] gives branch from root, and
    // so on.
    BSet branch_;

    // Zero-based depth of path into tree.
    unsigned depth_;

    // Handles of nodes in path from root to current node (returned by *).
    static const size_t kPathSize = maxDepth - 1;
    handle path_h_[kPathSize];

    int CmpKN(key k, handle h) { return tree_->abs_.CompareKeyNode(k, h); }
    int CmpNN(handle h1, handle h2) {
      return tree_->abs_.CompareNodeNode(h1, h2);
    }
    handle GetLT(handle h) { return tree_->abs_.GetLess(h); }
    handle GetGT(handle h) { return tree_->abs_.GetGreater(h); }
    handle Null() { return tree_->abs_.Null(); }
  };

  template <typename fwd_iter>
  bool Build(fwd_iter p, size num_nodes) {
    if (num_nodes == 0) {
      abs_.root = Null();
      return true;
    }

    // Gives path to subtree being built.  If branch[N] is false, branch
    // less from the node at depth N, if true branch greater.
    BSet branch;

    // If rem[N] is true, then for the current subtree at depth N, it's
    // greater subtree has one more node than it's less subtree.
    BSet rem;

    // Depth of root node of current subtree.
    unsigned depth = 0;

    // Number of nodes in current subtree.
    size num_sub = num_nodes;

    // The algorithm relies on a stack of nodes whose less subtree has
    // been built, but whose right subtree has not yet been built.  The
    // stack is implemented as linked list.  The nodes are linked
    // together by having the "greater" handle of a node set to the
    // next node in the list.  "less_parent" is the handle of the first
    // node in the list.
    handle less_parent = Null();

    // h is root of current subtree, child is one of its children.
    handle h, child;

    for (;;) {
      while (num_sub > 2) {
        // Subtract one for root of subtree.
        num_sub--;
        rem[depth] = !!(num_sub & 1);
        branch[depth++] = false;
        num_sub >>= 1;
      }

      if (num_sub == 2) {
        // Build a subtree with two nodes, slanting to greater.
        // I arbitrarily chose to always have the extra node in the
        // greater subtree when there is an odd number of nodes to
        // split between the two subtrees.

        h = *p;
        p++;
        child = *p;
        p++;
        SetLT(child, Null());
        SetGT(child, Null());
        SetBF(child, 0);
        SetGT(h, child);
        SetLT(h, Null());
        SetBF(h, 1);
      } else {  // num_sub == 1
                // Build a subtree with one node.

        h = *p;
        p++;
        SetLT(h, Null());
        SetGT(h, Null());
        SetBF(h, 0);
      }

      while (depth) {
        depth--;
        if (!branch[depth]) {
          // We've completed a less subtree.
          break;
        }

        // We've completed a greater subtree, so attach it to
        // its parent (that is less than it).  We pop the parent
        // off the stack of less parents.
        child = h;
        h = less_parent;
        less_parent = GetGT(h);
        SetGT(h, child);
        // num_sub = 2 * (num_sub - rem[depth]) + rem[depth] + 1
        num_sub <<= 1;
        num_sub += 1 - rem[depth];
        if (num_sub & (num_sub - 1)) {
          // num_sub is not a power of 2
          SetBF(h, 0);
        } else {
          // num_sub is a power of 2
          SetBF(h, 1);
        }
      }

      if (num_sub == num_nodes) {
        // We've completed the full tree.
        break;
      }

      // The subtree we've completed is the less subtree of the
      // next node in the sequence.

      child = h;
      h = *p;
      p++;
      SetLT(h, child);

      // Put h into stack of less parents.
      SetGT(h, less_parent);
      less_parent = h;

      // Proceed to creating greater than subtree of h.
      branch[depth] = true;
      num_sub += rem[depth++];
    }  // end for (;;)

    abs_.root = h;

    return true;
  }

 protected:
  friend class Iterator;

  // Create a class whose sole purpose is to take advantage of
  // the "empty member" optimization.
  struct abs_plus_root : public Abstractor {
    // The handle of the root element in the AVL tree.
    handle root;
  };

  abs_plus_root abs_;

  handle GetLT(handle h) { return abs_.GetLess(h); }
  void SetLT(handle h, handle lh) { abs_.SetLess(h, lh); }

  handle GetGT(handle h) { return abs_.GetGreater(h); }
  void SetGT(handle h, handle gh) { abs_.SetGreater(h, gh); }

  int GetBF(handle h) { return abs_.GetBalanceFactor(h); }
  void SetBF(handle h, int bf) { abs_.SetBalanceFactor(h, bf); }

  int CmpKN(key k, handle h) { return abs_.CompareKeyNode(k, h); }
  int CmpNN(handle h1, handle h2) { return abs_.CompareNodeNode(h1, h2); }

  handle Null() { return abs_.Null(); }

 private:
  // Balances subtree, returns handle of root node of subtree
  // after balancing.
  handle Balance(handle bal_h) {
    handle deep_h;

    // Either the "greater than" or the "less than" subtree of
    // this node has to be 2 levels deeper (or else it wouldn't
    // need balancing).

    if (GetBF(bal_h) > 0) {
      // "Greater than" subtree is deeper.

      deep_h = GetGT(bal_h);

      if (GetBF(deep_h) < 0) {
        handle old_h = bal_h;
        bal_h = GetLT(deep_h);

        SetGT(old_h, GetLT(bal_h));
        SetLT(deep_h, GetGT(bal_h));
        SetLT(bal_h, old_h);
        SetGT(bal_h, deep_h);

        int bf = GetBF(bal_h);
        if (bf != 0) {
          if (bf > 0) {
            SetBF(old_h, -1);
            SetBF(deep_h, 0);
          } else {
            SetBF(deep_h, 1);
            SetBF(old_h, 0);
          }
          SetBF(bal_h, 0);
        } else {
          SetBF(old_h, 0);
          SetBF(deep_h, 0);
        }
      } else {
        SetGT(bal_h, GetLT(deep_h));
        SetLT(deep_h, bal_h);
        if (GetBF(deep_h) == 0) {
          SetBF(deep_h, -1);
          SetBF(bal_h, 1);
        } else {
          SetBF(deep_h, 0);
          SetBF(bal_h, 0);
        }
        bal_h = deep_h;
      }
    } else {
      // "Less than" subtree is deeper.

      deep_h = GetLT(bal_h);

      if (GetBF(deep_h) > 0) {
        handle old_h = bal_h;
        bal_h = GetGT(deep_h);
        SetLT(old_h, GetGT(bal_h));
        SetGT(deep_h, GetLT(bal_h));
        SetGT(bal_h, old_h);
        SetLT(bal_h, deep_h);

        int bf = GetBF(bal_h);
        if (bf != 0) {
          if (bf < 0) {
            SetBF(old_h, 1);
            SetBF(deep_h, 0);
          } else {
            SetBF(deep_h, -1);
            SetBF(old_h, 0);
          }
          SetBF(bal_h, 0);
        } else {
          SetBF(old_h, 0);
          SetBF(deep_h, 0);
        }
      } else {
        SetLT(bal_h, GetGT(deep_h));
        SetGT(deep_h, bal_h);
        if (GetBF(deep_h) == 0) {
          SetBF(deep_h, 1);
          SetBF(bal_h, -1);
        } else {
          SetBF(deep_h, 0);
          SetBF(bal_h, 0);
        }
        bal_h = deep_h;
      }
    }

    return bal_h;
  }
};

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::Insert(handle h) {
  SetLT(h, Null());
  SetGT(h, Null());
  SetBF(h, 0);

  if (abs_.root == Null()) {
    abs_.root = h;
  } else {
    // Last unbalanced node encountered in search for insertion point.
    handle unbal = Null();
    // Parent of last unbalanced node.
    handle parent_unbal = Null();
    // Balance factor of last unbalanced node.
    int unbal_bf;

    // Zero-based depth in tree.
    unsigned depth = 0, unbal_depth = 0;

    // Records a path into the tree.  If branch[n] is true, indicates
    // take greater branch from the nth node in the path, otherwise
    // take the less branch.  branch[0] gives branch from root, and
    // so on.
    BSet branch;

    handle hh = abs_.root;
    handle parent = Null();
    int cmp;

    do {
      if (GetBF(hh) != 0) {
        unbal = hh;
        parent_unbal = parent;
        unbal_depth = depth;
      }
      cmp = CmpNN(h, hh);
      if (cmp == 0) {
        // Duplicate key.
        return hh;
      }
      parent = hh;
      hh = cmp < 0 ? GetLT(hh) : GetGT(hh);
      branch[depth++] = cmp > 0;
    } while (hh != Null());

    //  Add node to insert as leaf of tree.
    if (cmp < 0)
      SetLT(parent, h);
    else
      SetGT(parent, h);

    depth = unbal_depth;

    if (unbal == Null()) {
      hh = abs_.root;
    } else {
      cmp = branch[depth++] ? 1 : -1;
      unbal_bf = GetBF(unbal);
      if (cmp < 0)
        unbal_bf--;
      else  // cmp > 0
        unbal_bf++;
      hh = cmp < 0 ? GetLT(unbal) : GetGT(unbal);
      if ((unbal_bf != -2) && (unbal_bf != 2)) {
        // No rebalancing of tree is necessary.
        SetBF(unbal, unbal_bf);
        unbal = Null();
      }
    }

    if (hh != Null()) {
      while (h != hh) {
        cmp = branch[depth++] ? 1 : -1;
        if (cmp < 0) {
          SetBF(hh, -1);
          hh = GetLT(hh);
        } else {  // cmp > 0
          SetBF(hh, 1);
          hh = GetGT(hh);
        }
      }
    }

    if (unbal != Null()) {
      unbal = Balance(unbal);
      if (parent_unbal == Null()) {
        abs_.root = unbal;
      } else {
        depth = unbal_depth - 1;
        cmp = branch[depth] ? 1 : -1;
        if (cmp < 0)
          SetLT(parent_unbal, unbal);
        else  // cmp > 0
          SetGT(parent_unbal, unbal);
      }
    }
  }

  return h;
}

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::Search(
    key k,
    typename AVLTree<Abstractor, maxDepth, BSet>::SearchType st) {
  const int kMaskHighBit = static_cast<int>(~((~(unsigned)0) >> 1));

  int cmp, target_cmp;
  handle match_h = Null();
  handle h = abs_.root;

  if (st & LESS)
    target_cmp = 1;
  else if (st & GREATER)
    target_cmp = -1;
  else
    target_cmp = 0;

  while (h != Null()) {
    cmp = CmpKN(k, h);
    if (cmp == 0) {
      if (st & EQUAL) {
        match_h = h;
        break;
      }
      cmp = -target_cmp;
    } else if (target_cmp != 0) {
      if (!((cmp ^ target_cmp) & kMaskHighBit)) {
        // cmp and target_cmp are both positive or both negative.
        match_h = h;
      }
    }
    h = cmp < 0 ? GetLT(h) : GetGT(h);
  }

  return match_h;
}

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::SearchLeast() {
  handle h = abs_.root, parent = Null();

  while (h != Null()) {
    parent = h;
    h = GetLT(h);
  }

  return parent;
}

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::SearchGreatest() {
  handle h = abs_.root, parent = Null();

  while (h != Null()) {
    parent = h;
    h = GetGT(h);
  }

  return parent;
}

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::Remove(key k) {
  // Zero-based depth in tree.
  unsigned depth = 0, rm_depth;

  // Records a path into the tree.  If branch[n] is true, indicates
  // take greater branch from the nth node in the path, otherwise
  // take the less branch.  branch[0] gives branch from root, and
  // so on.
  BSet branch;

  handle h = abs_.root;
  handle parent = Null(), child;
  int cmp, cmp_shortened_sub_with_path = 0;

  for (;;) {
    if (h == Null()) {
      // No node in tree with given key.
      return Null();
    }
    cmp = CmpKN(k, h);
    if (cmp == 0) {
      // Found node to remove.
      break;
    }
    parent = h;
    h = cmp < 0 ? GetLT(h) : GetGT(h);
    branch[depth++] = cmp > 0;
    cmp_shortened_sub_with_path = cmp;
  }
  handle rm = h;
  handle parent_rm = parent;
  rm_depth = depth;

  // If the node to remove is not a leaf node, we need to get a
  // leaf node, or a node with a single leaf as its child, to put
  // in the place of the node to remove.  We will get the greatest
  // node in the less subtree (of the node to remove), or the least
  // node in the greater subtree.  We take the leaf node from the
  // deeper subtree, if there is one.

  if (GetBF(h) < 0) {
    child = GetLT(h);
    branch[depth] = false;
    cmp = -1;
  } else {
    child = GetGT(h);
    branch[depth] = true;
    cmp = 1;
  }
  depth++;

  if (child != Null()) {
    cmp = -cmp;
    do {
      parent = h;
      h = child;
      if (cmp < 0) {
        child = GetLT(h);
        branch[depth] = false;
      } else {
        child = GetGT(h);
        branch[depth] = true;
      }
      depth++;
    } while (child != Null());

    if (parent == rm) {
      // Only went through do loop once.  Deleted node will be replaced
      // in the tree structure by one of its immediate children.
      cmp_shortened_sub_with_path = -cmp;
    } else {
      cmp_shortened_sub_with_path = cmp;
    }

    // Get the handle of the opposite child, which may not be null.
    child = cmp > 0 ? GetLT(h) : GetGT(h);
  }

  if (parent == Null()) {
    // There were only 1 or 2 nodes in this tree.
    abs_.root = child;
  } else if (cmp_shortened_sub_with_path < 0) {
    SetLT(parent, child);
  } else {
    SetGT(parent, child);
  }

  // "path" is the parent of the subtree being eliminated or reduced
  // from a depth of 2 to 1.  If "path" is the node to be removed, we
  // set path to the node we're about to poke into the position of the
  // node to be removed.
  handle path = parent == rm ? h : parent;

  if (h != rm) {
    // Poke in the replacement for the node to be removed.
    SetLT(h, GetLT(rm));
    SetGT(h, GetGT(rm));
    SetBF(h, GetBF(rm));
    if (parent_rm == Null()) {
      abs_.root = h;
    } else {
      depth = rm_depth - 1;
      if (branch[depth])
        SetGT(parent_rm, h);
      else
        SetLT(parent_rm, h);
    }
  }

  if (path != Null()) {
    // Create a temporary linked list from the parent of the path node
    // to the root node.
    h = abs_.root;
    parent = Null();
    depth = 0;
    while (h != path) {
      if (branch[depth++]) {
        child = GetGT(h);
        SetGT(h, parent);
      } else {
        child = GetLT(h);
        SetLT(h, parent);
      }
      parent = h;
      h = child;
    }

    // Climb from the path node to the root node using the linked
    // list, restoring the tree structure and rebalancing as necessary.
    bool reduced_depth = true;
    int bf;
    cmp = cmp_shortened_sub_with_path;
    for (;;) {
      if (reduced_depth) {
        bf = GetBF(h);
        if (cmp < 0)
          bf++;
        else  // cmp > 0
          bf--;
        if ((bf == -2) || (bf == 2)) {
          h = Balance(h);
          bf = GetBF(h);
        } else {
          SetBF(h, bf);
        }
        reduced_depth = (bf == 0);
      }
      if (parent == Null())
        break;
      child = h;
      h = parent;
      cmp = branch[--depth] ? 1 : -1;
      if (cmp < 0) {
        parent = GetLT(h);
        SetLT(h, child);
      } else {
        parent = GetGT(h);
        SetGT(h, child);
      }
    }
    abs_.root = h;
  }

  return rm;
}

template <class Abstractor, unsigned maxDepth, class BSet>
inline typename AVLTree<Abstractor, maxDepth, BSet>::handle
AVLTree<Abstractor, maxDepth, BSet>::Subst(handle new_node) {
  handle h = abs_.root;
  handle parent = Null();
  int cmp, last_cmp;

  // Search for node already in tree with same key.
  for (;;) {
    if (h == Null()) {
      // No node in tree with same key as new node.
      return Null();
    }
    cmp = CmpNN(new_node, h);
    if (cmp == 0) {
      // Found the node to substitute new one for.
      break;
    }
    last_cmp = cmp;
    parent = h;
    h = cmp < 0 ? GetLT(h) : GetGT(h);
  }

  // Copy tree housekeeping fields from node in tree to new node.
  SetLT(new_node, GetLT(h));
  SetGT(new_node, GetGT(h));
  SetBF(new_node, GetBF(h));

  if (parent == Null()) {
    // New node is also new root.
    abs_.root = new_node;
  } else {
    // Make parent point to new node.
    if (last_cmp < 0)
      SetLT(parent, new_node);
    else
      SetGT(parent, new_node);
  }

  return h;
}

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_AVLTREE_H_
