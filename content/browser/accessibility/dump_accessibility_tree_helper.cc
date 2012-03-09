// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#include "base/memory/scoped_ptr.h"

namespace {
const int kIndentSpaces = 4;
}

void DumpAccessibilityTreeHelper::DumpAccessibilityTree(
    BrowserAccessibility* node, string16* contents) {
  RecursiveDumpAccessibilityTree(node, contents, 0);
}

void DumpAccessibilityTreeHelper::RecursiveDumpAccessibilityTree(
    BrowserAccessibility* node, string16* contents, int indent) {
  scoped_array<char> prefix(new char[indent + 1]);
  for (int i = 0; i < indent; ++i)
    prefix[i] = ' ';
  prefix[indent] = '\0';

  *contents += ToString(node, prefix.get());
  for (size_t i = 0; i < node->children().size(); ++i) {
    RecursiveDumpAccessibilityTree(node->children()[i], contents,
                                   indent + kIndentSpaces);
  }
}
