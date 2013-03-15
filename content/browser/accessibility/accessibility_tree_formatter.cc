// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace {
const int kIndentSpaces = 4;
const char* kSkipString = "@NO_DUMP";
}

AccessibilityTreeFormatter::AccessibilityTreeFormatter(
    BrowserAccessibility* node)
    : node_(node) {
  Initialize();
}

// static
AccessibilityTreeFormatter* AccessibilityTreeFormatter::Create(
    RenderViewHost* rvh) {
  RenderWidgetHostViewPort* host_view = static_cast<RenderWidgetHostViewPort*>(
      WebContents::FromRenderViewHost(rvh)->GetRenderWidgetHostView());

  BrowserAccessibilityManager* manager =
      host_view->GetBrowserAccessibilityManager();
  if (!manager)
    return NULL;

  BrowserAccessibility* root = manager->GetRoot();
  return new AccessibilityTreeFormatter(root);
}


AccessibilityTreeFormatter::~AccessibilityTreeFormatter() {
}

void AccessibilityTreeFormatter::FormatAccessibilityTree(
    string16* contents) {
  RecursiveFormatAccessibilityTree(node_, contents, 0);
}

void AccessibilityTreeFormatter::RecursiveFormatAccessibilityTree(
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
    RecursiveFormatAccessibilityTree(node->children()[i], contents,
                                     indent + kIndentSpaces);
  }
}

#if (!defined(OS_WIN) && !defined(OS_MACOSX))
string16 AccessibilityTreeFormatter::ToString(BrowserAccessibility* node,
                                              char* prefix) {
  return UTF8ToUTF16(prefix) + base::IntToString16(node->renderer_id()) +
       ASCIIToUTF16("\n");
}

void AccessibilityTreeFormatter::Initialize() {}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return FILE_PATH_LITERAL("");
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("");
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return "";
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return "";
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return "";
}
#endif

void AccessibilityTreeFormatter::SetFilters(
    const std::vector<Filter>& filters) {
  filters_ = filters;
}

bool AccessibilityTreeFormatter::MatchesFilters(
    const string16& text, bool default_result) const {
  std::vector<Filter>::const_iterator iter = filters_.begin();
  bool allow = default_result;
  for (iter = filters_.begin(); iter != filters_.end(); ++iter) {
    if (MatchPattern(text, iter->match_str)) {
      if (iter->type == Filter::ALLOW_EMPTY)
        allow = true;
      else if (iter->type == Filter::ALLOW)
        allow = (!MatchPattern(text, UTF8ToUTF16("*=''")));
      else
        allow = false;
    }
  }
  return allow;
}

void AccessibilityTreeFormatter::StartLine() {
  line_.clear();
}

void AccessibilityTreeFormatter::Add(
    bool include_by_default, const string16& attr) {
  if (attr.empty())
    return;
  if (!MatchesFilters(attr, include_by_default))
    return;
  if (!line_.empty())
    line_ += ASCIIToUTF16(" ");
  line_ += attr;
}

string16 AccessibilityTreeFormatter::FinishLine() {
  return line_;
}

}  // namespace content
