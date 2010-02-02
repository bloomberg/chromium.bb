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
  // Note that we don't use the simpler name Delegate as it seems to freak-out
  // the VisualStudio 2005 compiler in render_view.cc. (RenderView would be
  // implementing PageTranslator::Delegate that somehow confuses the compiler
  // into thinking we are talkin about TextTranslator::Delegate.)
  class PageTranslatorDelegate {
   public:
     virtual ~PageTranslatorDelegate() {}
     virtual void PageTranslated(int page_id,
                                 const std::string& original_lang,
                                 const std::string& target_lang) = 0;
  };

  // The caller remains the owner of |text_translator|.
  PageTranslator(TextTranslator* text_translator,
                 PageTranslatorDelegate* delegate);
  virtual ~PageTranslator();

  // Starts the translation process of |web_frame| from |source_lang| to
  // |target_lang| where the languages are the ISO codes (ex: en, fr...).
  void Translate(int page_id,
                 WebKit::WebFrame* web_frame,
                 std::string source_lang,
                 std::string target_lang);

  // Notification that the associated RenderView has navigated to a new page.
  void NavigatedToNewPage();

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
  typedef std::vector<string16> TextChunks;

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
  void ResetPageStates();

  // Clears any pending translation requests.  Any response for a pending
  // request received after this call will be ignored.
  void ClearPendingTranslations();

  // Our delegate (notified when a page is translated).
  PageTranslatorDelegate* delegate_;

  // The TextTranslator is responsible for translating the actual text chunks
  // from one language to another.
  TextTranslator* text_translator_;

  // The list of tags we are not interested in parsing when translating.
  std::set<WebKit::WebString, WebStringCompare> ignored_tags_;

  // The list of tags that do not break a block of text.
  std::set<WebKit::WebString, WebStringCompare> inline_tags_;

  // Mapping from a translation engine work id to the associated nodes.
  std::map<int, NodeList*> pending_translations_;

  // The language the page was in originally.
  std::string original_language_;

  // The language the page was translated to.
  std::string current_language_;

  // The page id of the page last time we translated.
  int page_id_;

  // True if the page is served over HTTPS.
  bool secure_page_;

  // The list of text zones in the current page, grouped in text zones (text
  // nodes grouped in a same context).
  std::vector<NodeList*> text_nodes_;
  // The original text of the text nodes in |text_nodes_|.
  std::vector<TextChunks*> text_chunks_;

  DISALLOW_COPY_AND_ASSIGN(PageTranslator);
};

#endif  // CHROME_RENDERER_TRANSLATE_PAGE_TRANSLATOR_H_
