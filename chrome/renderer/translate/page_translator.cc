// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/page_translator.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/renderer/navigation_state.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDataSource.h"
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

// A text node containing only characters in kIgnoredCharacters is not
// translated.
const char* const kIgnoredCharacters = ":,.[|]0123456789";

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

PageTranslator::PageTranslator(TextTranslator* text_translator,
                               PageTranslatorDelegate* delegate)
    : delegate_(delegate),
      text_translator_(text_translator),
      page_id_(-1),
      secure_page_(false) {
  for (size_t i = 0; i < arraysize(kSkippedTags); ++i)
    ignored_tags_.insert(WebKit::WebString(ASCIIToUTF16(kSkippedTags[i])));
  for (size_t i = 0; i < arraysize(kInlineTags); ++i)
    inline_tags_.insert(WebKit::WebString(ASCIIToUTF16(kInlineTags[i])));
  ignore_characters_ = ASCIIToUTF16(kIgnoredCharacters);
  ignore_characters_.append(kWhitespaceUTF16);
}

PageTranslator::~PageTranslator() {
  ResetPageStates();
}

void PageTranslator::TranslatePage(int page_id,
                                   WebKit::WebFrame* main_frame,
                                   std::string source_lang,
                                   std::string target_lang) {
  if (page_id != page_id_) {
    // This is a new page, our states are invalid.
    ResetPageStates();
    page_id_ = page_id;
    secure_page_ = static_cast<GURL>(main_frame->top()->url()).SchemeIsSecure();

    original_language_ = source_lang;
    current_language_ = target_lang;

    // Translate all frames contained within the main-frame.
    for (WebKit::WebFrame* frame = main_frame;
         frame; frame = frame->traverseNext(false)) {
      TranslateFrame(frame);
    }
    return;
  }

  // The page has already been translated, the text nodes are already available.
  DCHECK(!text_nodes_.empty());

  if (target_lang == original_language_) {
    // Special case where we want to revert to the original language.
    RevertTranslation();
    return;
  }

  // Any pending translation is now useless.
  ClearPendingTranslations();
  // No need to parse again the DOM, we have all the text nodes and their
  // original text in |text_nodes_| and |text_chunks_|.
  std::vector<NodeList*>::iterator text_nodes_iter = text_nodes_.begin();
  std::vector<TextChunks*>::iterator text_chunks_iter = text_chunks_.begin();
  for (;text_nodes_iter != text_nodes_.end();
       ++text_nodes_iter, ++text_chunks_iter) {
    DCHECK(text_chunks_iter != text_chunks_.end());
    int work_id = text_translator_->Translate(**text_chunks_iter,
                                              source_lang, target_lang,
                                              secure_page_, this);
    pending_translations_[work_id] = *text_nodes_iter;
  }
  current_language_ = target_lang;
}

void PageTranslator::TranslateFrame(WebKit::WebFrame* web_frame) {
  if (page_id_ == -1)
    return;  // The page has not been translated, ignore.

  DCHECK(!original_language_.empty() && !current_language_.empty());

  WebKit::WebDataSource* ds = web_frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  if (navigation_state->was_translated())
    return;  // This frame has already been translated, nothing to do.

  // If the frame has no document or an empty document, it may not have been
  // loaded yet.
  if (web_frame->document().isNull() ||
      web_frame->document().childNodes().length() == 0) {
    return;
  }

  std::stack<NodeList*> node_list_stack;
  std::vector<NodeList*> text_node_lists;
  TraverseNode(web_frame->document(), &node_list_stack, &text_node_lists);

  std::vector<NodeList*>::iterator iter;
  for (iter = text_node_lists.begin(); iter != text_node_lists.end(); ++iter) {
    if ((*iter)->empty()) {
      // Nothing to translate.
      continue;
    }
    TextChunks* text_chunks = new TextChunks;  // The text chunks to translate.
    NodeList::iterator text_nodes_iter;
    for (text_nodes_iter = (*iter)->begin();
         text_nodes_iter != (*iter)->end(); ++text_nodes_iter) {
      DCHECK(text_nodes_iter->isTextNode());
      string16 text = static_cast<string16>(text_nodes_iter->nodeValue());
      DCHECK(!ContainsOnlyWhitespace(text));
      text_chunks->push_back(text);
    }

    // Send the text for translation.
    int work_id =
        text_translator_->Translate(*text_chunks,
                                    original_language_, current_language_,
                                    secure_page_, this);
    pending_translations_[work_id] = *iter;
    // Also store the text nodes and their original text so we can translate to
    // another language if necessary.
    text_nodes_.push_back(*iter);
    text_chunks_.push_back(text_chunks);
  }

  navigation_state->set_was_translated(true);
}

void PageTranslator::MainFrameNavigated() {
  // We can drop all our states, they were related to the previous page.
  ResetPageStates();
}

bool PageTranslator::IsPageTranslated() {
  return original_language_ != current_language_;
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
    // The server might merge or split chunks in some cases.
    // TODO(jcampan): once the issue is resolved on the server, reenable that
    //                NOTREACHED().
    // NOTREACHED() << "Translation results received are inconsistent with the "
    //    "request";
    LOG(ERROR) << "translation response for work id " << work_id <<
        " length is " << translated_text_chunks.size() << " expected " <<
        nodes->size();
    ClearNodeZone(work_id);
    return;
  }

  for (size_t i = 0; i < translated_text_chunks.size(); ++i)
    (*nodes)[i].setNodeValue(WebKit::WebString(translated_text_chunks[i]));

  ClearNodeZone(work_id);

  if (delegate_ && pending_translations_.empty())
    delegate_->PageTranslated(page_id_, original_language_, current_language_);
}

void PageTranslator::TraverseNode(WebKit::WebNode node,
                                  std::stack<NodeList*>* element_stack,
                                  std::vector<NodeList*>* text_nodes_list) {
  if (node.isTextNode()) {
    string16 text = static_cast<string16>(node.nodeValue());
    if (ContainsOnlyChars(text, ignore_characters_))
      return;  // Ignore text nodes which contains only white-spaces or
               // separators.

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

void PageTranslator::ResetPageStates() {
  page_id_ = -1;
  secure_page_ = false;
  STLDeleteElements(&text_nodes_);
  STLDeleteElements(&text_chunks_);
  original_language_.clear();
  current_language_.clear();
  ClearPendingTranslations();
}

void PageTranslator::ClearPendingTranslations() {
  pending_translations_.clear();
}

void PageTranslator::RevertTranslation() {
  ClearPendingTranslations();

  DCHECK(!text_nodes_.empty());

  std::vector<NodeList*>::iterator text_nodes_iter = text_nodes_.begin();
  std::vector<TextChunks*>::iterator text_chunks_iter = text_chunks_.begin();
  for (;text_nodes_iter != text_nodes_.end();
       ++text_nodes_iter, ++text_chunks_iter) {
    DCHECK(text_chunks_iter != text_chunks_.end());
    DCHECK((*text_nodes_iter)->size() == (*text_chunks_iter)->size());
    NodeList::iterator node_iter = (*text_nodes_iter)->begin();
    TextChunks::const_iterator text_iter = (*text_chunks_iter)->begin();
    for (; node_iter != (*text_nodes_iter)->end(); ++node_iter, ++text_iter) {
      node_iter->setNodeValue(*text_iter);
    }
  }
  current_language_ = original_language_;
}
