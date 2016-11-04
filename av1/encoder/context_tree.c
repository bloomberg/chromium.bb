/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"

static const BLOCK_SIZE square[] = {
  BLOCK_8X8, BLOCK_16X16, BLOCK_32X32, BLOCK_64X64,
};

static void alloc_mode_context(AV1_COMMON *cm, int num_4x4_blk,
                               PICK_MODE_CONTEXT *ctx) {
  const int num_blk = (num_4x4_blk < 4 ? 4 : num_4x4_blk);
  const int num_pix = num_blk << 4;
  int i;
  ctx->num_4x4_blk = num_blk;

  for (i = 0; i < MAX_MB_PLANE; ++i) {
    CHECK_MEM_ERROR(cm, ctx->coeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->coeff[i])));
    CHECK_MEM_ERROR(cm, ctx->qcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->qcoeff[i])));
    CHECK_MEM_ERROR(cm, ctx->dqcoeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->dqcoeff[i])));
#if CONFIG_PVQ
    CHECK_MEM_ERROR(cm, ctx->pvq_ref_coeff[i],
                    aom_memalign(32, num_pix * sizeof(*ctx->pvq_ref_coeff[i])));
#endif
    CHECK_MEM_ERROR(cm, ctx->eobs[i],
                    aom_memalign(32, num_blk * sizeof(*ctx->eobs[i])));
  }

#if CONFIG_PALETTE
  if (cm->allow_screen_content_tools) {
    for (i = 0; i < 2; ++i) {
      CHECK_MEM_ERROR(
          cm, ctx->color_index_map[i],
          aom_memalign(32, num_pix * sizeof(*ctx->color_index_map[i])));
    }
  }
#endif  // CONFIG_PALETTE
}

static void free_mode_context(PICK_MODE_CONTEXT *ctx) {
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(ctx->coeff[i]);
    ctx->coeff[i] = 0;
    aom_free(ctx->qcoeff[i]);
    ctx->qcoeff[i] = 0;
    aom_free(ctx->dqcoeff[i]);
    ctx->dqcoeff[i] = 0;
#if CONFIG_PVQ
    aom_free(ctx->pvq_ref_coeff[i]);
    ctx->pvq_ref_coeff[i] = 0;
#endif
    aom_free(ctx->eobs[i]);
    ctx->eobs[i] = 0;
  }
#if CONFIG_PALETTE
  for (i = 0; i < 2; ++i) {
    aom_free(ctx->color_index_map[i]);
    ctx->color_index_map[i] = NULL;
  }
#endif  // CONFIG_PALETTE
}

static void alloc_tree_contexts(AV1_COMMON *cm, PC_TREE *tree,
                                int num_4x4_blk) {
  alloc_mode_context(cm, num_4x4_blk, &tree->none);
  alloc_mode_context(cm, num_4x4_blk / 2, &tree->horizontal[0]);
  alloc_mode_context(cm, num_4x4_blk / 2, &tree->vertical[0]);

  if (num_4x4_blk > 4) {
    alloc_mode_context(cm, num_4x4_blk / 2, &tree->horizontal[1]);
    alloc_mode_context(cm, num_4x4_blk / 2, &tree->vertical[1]);
  } else {
    memset(&tree->horizontal[1], 0, sizeof(tree->horizontal[1]));
    memset(&tree->vertical[1], 0, sizeof(tree->vertical[1]));
  }
}

static void free_tree_contexts(PC_TREE *tree) {
  free_mode_context(&tree->none);
  free_mode_context(&tree->horizontal[0]);
  free_mode_context(&tree->horizontal[1]);
  free_mode_context(&tree->vertical[0]);
  free_mode_context(&tree->vertical[1]);
}

// This function sets up a tree of contexts such that at each square
// partition level. There are contexts for none, horizontal, vertical, and
// split.  Along with a block_size value and a selected block_size which
// represents the state of our search.
void av1_setup_pc_tree(AV1_COMMON *cm, ThreadData *td) {
  int i, j;
  const int leaf_nodes = 64;
  const int tree_nodes = 64 + 16 + 4 + 1;
  int pc_tree_index = 0;
  PC_TREE *this_pc;
  PICK_MODE_CONTEXT *this_leaf;
  int square_index = 1;
  int nodes;

  aom_free(td->leaf_tree);
  CHECK_MEM_ERROR(cm, td->leaf_tree,
                  aom_calloc(leaf_nodes, sizeof(*td->leaf_tree)));
  aom_free(td->pc_tree);
  CHECK_MEM_ERROR(cm, td->pc_tree,
                  aom_calloc(tree_nodes, sizeof(*td->pc_tree)));

  this_pc = &td->pc_tree[0];
  this_leaf = &td->leaf_tree[0];

  // 4x4 blocks smaller than 8x8 but in the same 8x8 block share the same
  // context so we only need to allocate 1 for each 8x8 block.
  for (i = 0; i < leaf_nodes; ++i) alloc_mode_context(cm, 1, &td->leaf_tree[i]);

  // Sets up all the leaf nodes in the tree.
  for (pc_tree_index = 0; pc_tree_index < leaf_nodes; ++pc_tree_index) {
    PC_TREE *const tree = &td->pc_tree[pc_tree_index];
    tree->block_size = square[0];
    alloc_tree_contexts(cm, tree, 4);
    tree->leaf_split[0] = this_leaf++;
    for (j = 1; j < 4; j++) tree->leaf_split[j] = tree->leaf_split[0];
  }

  // Each node has 4 leaf nodes, fill each block_size level of the tree
  // from leafs to the root.
  for (nodes = 16; nodes > 0; nodes >>= 2) {
    for (i = 0; i < nodes; ++i) {
      PC_TREE *const tree = &td->pc_tree[pc_tree_index];
      alloc_tree_contexts(cm, tree, 4 << (2 * square_index));
      tree->block_size = square[square_index];
      for (j = 0; j < 4; j++) tree->split[j] = this_pc++;
      ++pc_tree_index;
    }
    ++square_index;
  }
  td->pc_root = &td->pc_tree[tree_nodes - 1];
  td->pc_root[0].none.best_mode_index = 2;
}

void av1_free_pc_tree(ThreadData *td) {
  const int tree_nodes = 64 + 16 + 4 + 1;
  int i;

  // Set up all 4x4 mode contexts
  for (i = 0; i < 64; ++i) free_mode_context(&td->leaf_tree[i]);

  // Sets up all the leaf nodes in the tree.
  for (i = 0; i < tree_nodes; ++i) free_tree_contexts(&td->pc_tree[i]);

  aom_free(td->pc_tree);
  td->pc_tree = NULL;
  aom_free(td->leaf_tree);
  td->leaf_tree = NULL;
}
