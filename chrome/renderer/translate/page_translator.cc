// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/page_translator.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/task.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNodeList.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

namespace {

// The following elements are not supposed to be translated.
const char* const kSkippedTags[] = { "APPLET", "AREA", "BASE", "FRAME",
    "FRAMESET", "HR", "IFRAME", "IMG", "INPUT", "LINK", "META", "MAP",
    "OBJECT", "PARAM", "SCRIPT", "STYLE", "TEXTAREA" };

// The following tags are not considered as breaking a block of text.
// Notes: does SPAN belong to this list?
const char* const kInlineTags[] = { "A", "ABBR", "ACRONYM", "B", "BIG", "DEL",
    "EM", "I", "INS", "S", "SPAN", "STRIKE", "STRONG", "SUB", "SUP", "U" };
}

// Returns true when s1 < s2.
bool PageTranslator::WebStringCompare::operator()(
    const WebKit::WebString& s1, const WebKit::WebString& s2) const {
  int len1 = s1.length();
  int len2 = s2.length();
  int r = base::strncmp16(s1.data(), s2.data(), std::min(len1, len2));

  if (r < 0)
    return true;
  else if (r > 0)
    return false;

  return len1 < len2;
}

PageTranslator::PageTranslator(TextTranslator* text_translator)
    : text_translator_(text_translator) {
  for (size_t i = 0; i < arraysize(kSkippedTags); ++i)
    ignored_tags_.insert(WebKit::WebString(ASCIIToUTF16(kSkippedTags[i])));
  for (size_t i = 0; i < arraysize(kInlineTags); ++i)
    inline_tags_.insert(WebKit::WebString(ASCIIToUTF16(kInlineTags[i])));
}

PageTranslator::~PageTranslator() {
  STLDeleteContainerPairSecondPointers(pending_translations_.begin(),
                                       pending_translations_.end());
}

void PageTranslator::Translate(WebKit::WebFrame* web_frame,
                               std::string from_lang,
                               std::string to_lang) {
  std::stack<NodeList*> node_list_stack;
  std::vector<NodeList*> text_node_lists;
  TraverseNode(web_frame->document(), &node_list_stack, &text_node_lists);

  std::vector<NodeList*>::iterator iter;
  for (iter = text_node_lists.begin(); iter != text_node_lists.end(); ++iter) {
    if ((*iter)->empty()) {
      // Nothing to translate.
      delete *iter;
      continue;
    }
    std::vector<string16> text_chunks;  // The text chunks to translate.
    std::vector<WebKit::WebNode>::iterator text_nodes_iter;
    for (text_nodes_iter = (*iter)->begin();
         text_nodes_iter != (*iter)->end(); ++text_nodes_iter) {
      DCHECK(text_nodes_iter->isTextNode());
      string16 text = static_cast<string16>(text_nodes_iter->nodeValue());
      DCHECK(!ContainsOnlyWhitespace(text));
      text_chunks.push_back(text);
    }
    // Send the text for translation.
    bool secure = static_cast<GURL>(web_frame->top()->url()).SchemeIsSecure();
    int work_id =
        text_translator_->Translate(text_chunks, from_lang, to_lang, secure,
                                    this);
    pending_translations_[work_id] = *iter;
  }
}

bool PageTranslator::ShouldElementBeTraversed(WebKit::WebElement element) {
  return ignored_tags_.find(element.tagName()) == ignored_tags_.end();
}

bool PageTranslator::IsInlineElement(WebKit::WebElement element) {
  return inline_tags_.find(element.tagName()) != inline_tags_.end();
}

void PageTranslator::ClearNodeZone(int work_id) {
  std::map<int, NodeList*>::iterator iter = pending_translations_.find(work_id);
  if (iter == pending_translations_.end()) {
    NOTREACHED() << "Clearing unknown node zone in pending_translations_, "
        "work id=" << work_id;
    return;
  }
  delete iter->second;
  pending_translations_.erase(iter);
}

void PageTranslator::TranslationError(int work_id, int error_id) {
  // TODO(jcampan): may be we should show somehow that something went wrong to
  //                the user?
  ClearNodeZone(work_id);
}

void PageTranslator::TextTranslated(
    int work_id, const std::vector<string16>& translated_text_chunks) {
  std::map<int, NodeList*>::iterator iter = pending_translations_.find(work_id);
  if (iter == pending_translations_.end()) {
    // We received some translated text we were not expecting.  It could be we
    // navigated away from the page or that the translation was undone.
    return;
  }

  NodeList* nodes = iter->second;
  // Check the integrity of the response.
  if (translated_text_chunks.size() != nodes->size()) {
    // TODO(jcampan) reenable when we figured out why the server messed up the
    // anchor tags.
    // NOTREACHED() << "Translation results received are inconsistent with the "
    //    "request";
    return;
  }

  for (size_t i = 0; i < translated_text_chunks.size(); ++i)
    (*nodes)[i].setNodeValue(WebKit::WebString(translated_text_chunks[i]));

  ClearNodeZone(work_id);
}

void PageTranslator::TraverseNode(WebKit::WebNode node,
                                  std::stack<NodeList*>* element_stack,
                                  std::vector<NodeList*>* text_nodes_list) {
  if (node.isTextNode()) {
    if (ContainsOnlyWhitespace(static_cast<string16>(node.nodeValue())))
      return;  // Ignore text nodes with only white-spaces.

    DCHECK(!element_stack->empty());
    NodeList* text_nodes = element_stack->top();
    if (text_nodes->empty()) {
      // This node zone is empty, meaning it has not yet been added to
      // |text_nodes|.
      text_nodes_list->push_back(text_nodes);
    }
    text_nodes->push_back(node);
    return;
  }

  if (!node.hasChildNodes())
    return;

  bool new_text_block = false;
  if (node.isElementNode()) {
    WebKit::WebElement element = node.toElement<WebKit::WebElement>();
    if (!ShouldElementBeTraversed(element))
      return;

    if (!IsInlineElement(element)) {
      new_text_block = true;
      NodeList* text_nodes = new NodeList();
      element_stack->push(text_nodes);
    }
  }

  WebKit::WebNodeList children = node.childNodes();
  for (size_t i = 0; i < children.length(); i++)
    TraverseNode(children.item(i), element_stack, text_nodes_list);

  if (new_text_block) {
    NodeList* text_nodes = element_stack->top();
    element_stack->pop();
    // If no nodes were added to text_nodes, then it has not been added to
    // text_nodes_list and must be deleted.
    if (text_nodes->empty())
      delete text_nodes;
  }
}
