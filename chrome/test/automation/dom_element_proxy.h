// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_
#define CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

class DOMElementProxy;
class JavaScriptExecutionController;

typedef scoped_refptr<DOMElementProxy> DOMElementProxyRef;

// This class is a proxy to an object in JavaScript. It holds a handle which
// can be used to retrieve the actual object in JavaScript scripts.
class JavaScriptObjectProxy
    : public base::RefCountedThreadSafe<JavaScriptObjectProxy> {
 public:
  JavaScriptObjectProxy(JavaScriptExecutionController* executor, int handle);
  ~JavaScriptObjectProxy();

  int handle() const { return handle_; }
  bool is_valid() const { return executor_.get() != NULL; }

 protected:
  base::WeakPtr<JavaScriptExecutionController> executor_;
  int handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaScriptObjectProxy);
};

// This class presents the interface to actions that can be performed on
// a given DOM element. Note that this object can be invalidated at any
// time. In that case, any subsequent calls will return false immediately.
// This class should never be instantiated directly, except by a
// JavaScriptExecutionController.
class DOMElementProxy : public JavaScriptObjectProxy {
 public:
  // This class represents the various methods by which elements are located
  // in the DOM.
  class By {
   public:
    enum ByType {
      TYPE_XPATH,
      TYPE_SELECTORS,
      TYPE_TEXT
    };

    // Returns a By for locating an element using an XPath query.
    static By XPath(const std::string& xpath);

    // Returns a By for locating an element using CSS selectors.
    static By Selectors(const std::string& selectors);

    // Returns a By for locating an element by its contained text. For inputs
    // and textareas, this includes the element's value.
    static By Text(const std::string& text);

    ByType type() const { return type_; }
    std::string query() const { return query_; }

   private:
    By(ByType type, const std::string& query)
        : type_(type), query_(query) {}

    ByType type_;
    std::string query_;
  };

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
  DOMElementProxyRef GetDocumentFromFrame(const std::string& frame_name1,
                                          const std::string& frame_name2 = "",
                                          const std::string& frame_name3 = "");

  // Finds the first element found by the given locator method |by|, or NULL
  // if no element was found.
  DOMElementProxyRef FindElement(const By& by);

  // Finds all the elements found by the given locator method and appends
  // them to the given list. Returns true on success.
  bool FindElements(const By& by,
                    std::vector<DOMElementProxyRef>* elements);

  // Waits until the number of visible elements satisfying the given locator
  // method |by| equals |count|, and appends them to the given list. Returns
  // true when |count| matches the number of visible elements or false if
  // the timeout is exceeded while waiting. If false, the list is not modified.
  bool WaitForVisibleElementCount(const By& by, int count,
                                  std::vector<DOMElementProxyRef>* elements);

  // Waits until exactly 1 element is visible which satisifies the given
  // locator method. Returns the found element, or NULL if the timeout is
  // exceeded. If it is possible for more than 1 element to safisfy the query,
  // use WaitForVisibleElementCount instead.
  DOMElementProxyRef WaitFor1VisibleElement(const By& by);

  // Waits until no visible elements satisify the given locator method.
  // Returns true when no more visible elements are found or false if the
  // timeout is exceeded while waiting.
  bool WaitForElementsToDisappear(const By& by);

  // Dispatches a click MouseEvent to this element and all its parents.
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

  // Returns if |expected_value| eventually matches the element's value for
  // |attribute|. This function will block until the timeout is exceeded, in
  // which case it will fail, or until the two values match.
  bool DoesAttributeEventuallyMatch(const std::string& attribute,
                                    const std::string& expected_value);

 private:
  // Gets the element's value for the given type. This is a helper method
  // for simple get methods.
  template <typename T>
  bool GetValue(const std::string& type, T* out);

  DISALLOW_COPY_AND_ASSIGN(DOMElementProxy);
};

#endif  // CHROME_TEST_AUTOMATION_DOM_ELEMENT_PROXY_H_
