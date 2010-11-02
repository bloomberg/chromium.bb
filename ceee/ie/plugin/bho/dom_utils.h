// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A collection of utility functions that interrogate or mutate the IE DOM.
#ifndef CEEE_IE_PLUGIN_BHO_DOM_UTILS_H_
#define CEEE_IE_PLUGIN_BHO_DOM_UTILS_H_

#include <mshtml.h>
#include <string>

// This class is a namespace for hosting utility functions that interrogate
// or mutate the IE DOM.
// TODO(siggi@chromium.org): should this be a namespace?
class DomUtils {
 public:
  // Inject a style tag into the head of the document.
  // @param document The DOM document object.
  // @param head_node The HEAD DOM node from |document|.
  // @param code The CSS code to inject.
  static HRESULT InjectStyleTag(IHTMLDocument2* document,
                                IHTMLDOMNode* head_node,
                                const wchar_t* code);

  // Retrieve the "HEAD" DOM node from @p document.
  // @param document the DOM document object.
  // @param head_node on success returns the "HEAD" DOM node.
  static HRESULT GetHeadNode(IHTMLDocument* document, IHTMLDOMNode** head_node);

  // Retrieves all elements with the specified tag name from the document.
  // @param document The document object.
  // @param tag_name The tag name.
  // @param elements On success returns a collection of elements.
  // @param length On success returns the number of elements in the collection.
  //               The caller could pass in NULL to indicate that length
  //               information is not needed.
  static HRESULT GetElementsByTagName(IHTMLDocument3* document,
                                      BSTR tag_name,
                                      IHTMLElementCollection** elements,
                                      long* length);

  // Retrieves an element from the collection.
  // @param collection The collection object.
  // @param index The zero-based index of the element to retrieve.
  // @param id The interface ID of the element to retrieve.
  // @param element On success returns the element.
  static HRESULT GetElementFromCollection(IHTMLElementCollection* collection,
                                          long index,
                                          REFIID id,
                                          void** element);
};

#endif  // CEEE_IE_PLUGIN_BHO_DOM_UTILS_H_
