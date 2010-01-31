// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_PAGE_TRANSLATOR_H_
#define CHROME_RENDERER_TRANSLATE_PAGE_TRANSLATOR_H_

#include <map>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/renderer/translate/text_translator.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"

class RenderView;

namespace WebKit {
class WebFrame;
class WebNode;
class WebString;
}

// The PageTranslator is a service that translates the text content of a web
// page from one language to another (ex: English to French).
// It performs the traversal of the DOM of the page to retrieve the text nodes
// and delegates the actual text translation to a TextTranslator.
class PageTranslator : public TextTranslator::Delegate {
 public:
  // The caller remains the owner of |text_translator|.
  explicit PageTranslator(TextTranslator* text_translator);
  virtual ~PageTranslator();

  // Starts the translation process of |web_frame| from |from_lang| to |to_lang|
  // where the languages are the ISO codes (ex: en, fr...).
  void Translate(WebKit::WebFrame* web_frame,
                 std::string from_lang,
                 std::string to_lang);

  // Notification that the associated RenderView has navigated to a new page.
  void NavigatedToNewPage();

  // Reverts the page to its original non-translated contents.
  void UndoTranslation();

  // TextTranslator::Delegate implentation:
  virtual void TranslationError(int work_id, int error_id);
  virtual void TextTranslated(
      int work_id, const std::vector<string16>& translated_text);

 private:
  // Comparator used in set of WebKit WebStrings.
  struct WebStringCompare {
    bool operator()(const WebKit::WebString& s1,
                    const WebKit::WebString& s2) const;
  };

  typedef std::vector<WebKit::WebNode> NodeList;

  // Traverses the tree starting at |node| and fills |nodes| with the
  // elements necessary for translation.
  // |element_stack| is used to retrieve the current node list during the tree
  // traversal.
  void TraverseNode(WebKit::WebNode node,
                    std::stack<NodeList*>* element_stack,
                    std::vector<NodeList*>* nodes);

  // Whether this |element| should be parsed or ignored for translation purpose.
  bool ShouldElementBeTraversed(WebKit::WebElement element);

  // Whether this element should be considered as part of the other text nodes
  // at the same hiearchical level.
  bool IsInlineElement(WebKit::WebElement element);

  // Removes and deletes the NodeZone for |work_id| in pending_translations_.
  void ClearNodeZone(int work_id);

  // Clears all the states related to the page's contents.
  void ResetPageState();

  // The RenderView we are providing translations for.
  RenderView* render_view_;

  // The TextTranslator is responsible for translating the actual text chunks
  // from one language to another.
  TextTranslator* text_translator_;

  // The list of tags we are not interested in parsing when translating.
  std::set<WebKit::WebString, WebStringCompare> ignored_tags_;

  // The list of tags that do not break a block of text.
  std::set<WebKit::WebString, WebStringCompare> inline_tags_;

  // Mapping from a translation engine work id to the associated nodes.
  std::map<int, NodeList*> pending_translations_;

  // The list of text nodes in the current page with their original text.
  // Used to undo the translation.
  typedef std::pair<WebKit::WebNode, WebKit::WebString> NodeTextPair;
  std::vector<NodeTextPair> text_nodes_;

  DISALLOW_COPY_AND_ASSIGN(PageTranslator);
};

#endif  // CHROME_RENDERER_TRANSLATE_PAGE_TRANSLATOR_H_
