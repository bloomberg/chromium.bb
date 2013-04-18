// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
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
const char* kChildrenDictAttr = "children";
}

AccessibilityTreeFormatter::AccessibilityTreeFormatter(
    BrowserAccessibility* root)
    : root_(root) {
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

scoped_ptr<DictionaryValue>
AccessibilityTreeFormatter::BuildAccessibilityTree() {
  scoped_ptr<DictionaryValue> dict(new DictionaryValue);
  RecursiveBuildAccessibilityTree(*root_, dict.get());
  return dict.Pass();
}

void AccessibilityTreeFormatter::FormatAccessibilityTree(
    string16* contents) {
  scoped_ptr<DictionaryValue> dict = BuildAccessibilityTree();
  RecursiveFormatAccessibilityTree(*(dict.get()), contents);
}

void AccessibilityTreeFormatter::RecursiveBuildAccessibilityTree(
    const BrowserAccessibility& node, DictionaryValue* dict) {
  AddProperties(node, dict);
  ListValue* children = new ListValue;
  dict->Set(kChildrenDictAttr, children);
  for (size_t i = 0; i < node.children().size(); ++i) {
    BrowserAccessibility* child_node = node.children()[i];
    DictionaryValue* child_dict = new DictionaryValue;
    children->Append(child_dict);
    RecursiveBuildAccessibilityTree(*child_node, child_dict);
  }
}

void AccessibilityTreeFormatter::RecursiveFormatAccessibilityTree(
    const DictionaryValue& dict, string16* contents, int depth) {
  string16 line = ToString(dict, string16(depth * kIndentSpaces, ' '));
  if (line.find(ASCIIToUTF16(kSkipString)) != string16::npos)
    return;

  *contents += line;
  const ListValue* children;
  dict.GetList(kChildrenDictAttr, &children);
  const DictionaryValue* child_dict;
  for (size_t i = 0; i < children->GetSize(); i++) {
    children->GetDictionary(i, &child_dict);
    RecursiveFormatAccessibilityTree(*child_dict, contents, depth + 1);
  }
}

#if (!defined(OS_WIN) && !defined(OS_MACOSX))
void AccessibilityTreeFormatter::AddProperties(const BrowserAccessibility& node,
                                               DictionaryValue* dict) {
  dict->SetInteger("id", node.renderer_id());
}

string16 AccessibilityTreeFormatter::ToString(const DictionaryValue& node,
                                              const string16& indent) {
  int id_value;
  node.GetInteger("id", &id_value);
  return indent + base::IntToString16(id_value) +
       ASCIIToUTF16("\n");
}

void AccessibilityTreeFormatter::Initialize() {}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetActualFileSuffix() {
  return base::FilePath::StringType();
}

// static
const base::FilePath::StringType
AccessibilityTreeFormatter::GetExpectedFileSuffix() {
  return base::FilePath::StringType();
}

// static
const std::string AccessibilityTreeFormatter::GetAllowEmptyString() {
  return std::string();
}

// static
const std::string AccessibilityTreeFormatter::GetAllowString() {
  return std::string();
}

// static
const std::string AccessibilityTreeFormatter::GetDenyString() {
  return std::string();
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

string16 AccessibilityTreeFormatter::FormatCoordinates(
    const char* name, const char* x_name, const char* y_name,
    const DictionaryValue& value) {
  int x, y;
  value.GetInteger(x_name, &x);
  value.GetInteger(y_name, &y);
  std::string xy_str(base::StringPrintf("%s=(%d, %d)", name, x, y));

  return UTF8ToUTF16(xy_str);
}

void AccessibilityTreeFormatter::WriteAttribute(
    bool include_by_default, const std::string& attr, string16* line) {
  WriteAttribute(include_by_default, UTF8ToUTF16(attr), line);
}

void AccessibilityTreeFormatter::WriteAttribute(
    bool include_by_default, const string16& attr, string16* line) {
  if (attr.empty())
    return;
  if (!MatchesFilters(attr, include_by_default))
    return;
  if (!line->empty())
    *line += ASCIIToUTF16(" ");
  *line += attr;
}

}  // namespace content
