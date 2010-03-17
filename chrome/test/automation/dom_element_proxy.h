// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_
#define CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "chrome/test/automation/javascript_execution_controller.h"

class DOMElementProxy;

typedef scoped_refptr<DOMElementProxy> DOMElementProxyRef;

// This class presents the interface to actions that can be performed on
// a given DOM element. Note that this object can be invalidated at any
// time. In that case, any subsequent calls will return false immediately.
class DOMElementProxy : public JavaScriptObjectProxy {
 public:
  DOMElementProxy(JavaScriptExecutionController* executor, int handle)
      : JavaScriptObjectProxy(executor, handle) {}

  // Returns the document for this element, which must be of type frame.
  // Returns NULL on failure.
  DOMElementProxyRef GetContentDocument();

  // Finds the frame which matches the list of given names, starting from
  // the window that contains this element. Each name in the list is used to
  // select the next sub frame. Returns NULL on failure.
  // A vector of "2" and "ad" is equivalent to the javascript:
  //     frame.frames["2"].frames["ad"].
  DOMElementProxyRef GetDocumentFromFrame(
      const std::vector<std::string>& frame_names);

  // Same as above but with different argument for convenience.
  DOMElementProxyRef GetDocumentFromFrame(const std::string& frame_name);

  // Same as above but with different argument for convenience.
  DOMElementProxyRef GetDocumentFromFrame(const std::string& frame_name1,
                                          const std::string& frame_name2);

  // Same as above but with different argument for convenience.
  DOMElementProxyRef GetDocumentFromFrame(const std::string& frame_name1,
      const std::string& frame_name2, const std::string& frame_name3);

  // Adds the elements from this element's descendants that satisfy the
  // XPath query |xpath| to the vector |elements|.
  // Returns true on success.
  bool FindByXPath(const std::string& xpath,
                   std::vector<DOMElementProxyRef>* elements);

  // Same as above, but returns the first element, or NULL if none.
  DOMElementProxyRef FindByXPath(const std::string& xpath);

  // Adds the elements from this element's descendants that match the
  // CSS Selectors |selectors| to the vector |elements|.
  // Returns true on success.
  bool FindBySelectors(const std::string& selectors,
                       std::vector<DOMElementProxyRef>* elements);

  // Same as above, but returns the first element, or NULL if none.
  DOMElementProxyRef FindBySelectors(const std::string& selectors);

  // Adds the elements from this element's descendants which have text that
  // matches |text|. This includes text from input elements.
  // Returns true on success.
  bool FindByText(const std::string& text,
                  std::vector<DOMElementProxyRef>* elements);

  // Same as above, but returns the first element, or NULL if none.
  DOMElementProxyRef FindByText(const std::string& text);

  // Dispatches a click MouseEvent to the element and all its parents.
  // Returns true on success.
  bool Click();

  // Adds |text| to this element. Only valid for textareas and textfields.
  // Returns true on success.
  bool Type(const std::string& text);

  // Sets the input text to |text|. Only valid for textareas and textfields.
  // Returns true on success.
  bool SetText(const std::string& text);

  // Gets the element's value for its |property|. Returns true on success.
  bool GetProperty(const std::string& property,
                   std::string* out);

  // Gets the element's value for its |attribute|. Returns true on success.
  bool GetAttribute(const std::string& attribute,
                    std::string* out);

  // Retrieves all the text in this element. This includes the value
  // of textfields and inputs. Returns true on success.
  bool GetText(std::string* text);

  // Retrieves the element's inner HTML. Returns true on success.
  bool GetInnerHTML(std::string* html);

  // Retrieves the element's id. Returns true on success.
  bool GetId(std::string* id);

  // Retrieves the element's name. Returns true on success.
  bool GetName(std::string* name);

  // Retrieves the element's visibility. Returns true on success.
  bool GetVisibility(bool* visilibity);

  // Asserts that |expected_text| matches all the text in this element. This
  // includes the value of textfields and inputs.
  void EnsureTextMatches(const std::string& expected_text);

  // Asserts that |expected_html| matches the element's inner html.
  void EnsureInnerHTMLMatches(const std::string& expected_html);

  // Asserts that |expected_name| matches the element's name.
  void EnsureNameMatches(const std::string& expected_name);

  // Asserts that |expected_visibility| matches the element's visibility.
  void EnsureVisibilityMatches(bool expected_visibility);
};

#endif  // CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_
