// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_tree_helper.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"

namespace {
const int kIndentSpaces = 4;
const char* kSkipString = "@NO_DUMP";
}

DumpAccessibilityTreeHelper::DumpAccessibilityTreeHelper() {
  Initialize();
}

DumpAccessibilityTreeHelper::~DumpAccessibilityTreeHelper() {
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

  string16 line = ToString(node, prefix.get());
  if (line.find(ASCIIToUTF16(kSkipString)) != string16::npos)
    return;

  *contents += line;
  for (size_t i = 0; i < node->children().size(); ++i) {
    RecursiveDumpAccessibilityTree(node->children()[i], contents,
                                   indent + kIndentSpaces);
  }
}

void DumpAccessibilityTreeHelper::SetFilters(
    const std::set<string16>& allow_filters,
    const std::set<string16>& deny_filters) {
  allow_filters_ = allow_filters;
  deny_filters_ = deny_filters;
}

bool DumpAccessibilityTreeHelper::MatchesFilters(
    const string16& text, bool default_result) {
  std::set<string16>::const_iterator iter = allow_filters_.begin();
  for (iter = allow_filters_.begin(); iter != allow_filters_.end(); ++iter) {
    if (MatchPattern(text, *iter))
      return true;
  }
  for (iter = deny_filters_.begin(); iter != deny_filters_.end(); ++iter) {
    if (MatchPattern(text, *iter))
      return false;
  }
  return default_result;
}

void DumpAccessibilityTreeHelper::StartLine() {
  line_.clear();
}

void DumpAccessibilityTreeHelper::Add(
    bool include_by_default, const string16& attr) {
  if (!MatchesFilters(attr, include_by_default))
    return;
  if (!line_.empty())
    line_ += ASCIIToUTF16(" ");
  line_ += attr;
}

string16 DumpAccessibilityTreeHelper::FinishLine() {
  return line_;
}
