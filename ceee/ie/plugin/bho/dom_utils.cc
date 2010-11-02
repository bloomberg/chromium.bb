// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A collection of utility functions that interrogate or mutate the IE DOM.
#include "ceee/ie/plugin/bho/dom_utils.h"

#include <atlbase.h>

#include "ceee/common/com_utils.h"
#include "base/logging.h"

HRESULT DomUtils::InjectStyleTag(IHTMLDocument2* document,
                                 IHTMLDOMNode* head_node,
                                 const wchar_t* code) {
  DCHECK(document != NULL);
  DCHECK(head_node != NULL);

  CComPtr<IHTMLElement> elem;
  HRESULT hr = document->createElement(CComBSTR(L"style"), &elem);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not create style element " << com::LogHr(hr);
    return hr;
  }

  CComQIPtr<IHTMLStyleElement> style_elem(elem);
  DCHECK(style_elem != NULL) <<
      "Could not QueryInterface for IHTMLStyleElement";

  hr = style_elem->put_type(CComBSTR(L"text/css"));
  DCHECK(SUCCEEDED(hr)) << "Could not set type of style element" <<
      com::LogHr(hr);

  CComPtr<IHTMLStyleSheet> style_sheet;
  hr = style_elem->get_styleSheet(&style_sheet);
  DCHECK(SUCCEEDED(hr)) << "Could not get styleSheet of style element." <<
      com::LogHr(hr);

  hr = style_sheet->put_cssText(CComBSTR(code));
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not set cssText of styleSheet." << com::LogHr(hr);
    return hr;
  }

  CComQIPtr<IHTMLDOMNode> style_node(style_elem);
  DCHECK(style_node != NULL) << "Could not query interface for IHTMLDomNode.";

  CComPtr<IHTMLDOMNode> dummy;
  hr = head_node->appendChild(style_node, &dummy);
  if (FAILED(hr))
    LOG(ERROR) << "Could not append style node to head node." << com::LogHr(hr);

  return hr;
}

HRESULT DomUtils::GetHeadNode(IHTMLDocument* document,
                              IHTMLDOMNode** head_node) {
  DCHECK(document != NULL);
  DCHECK(head_node != NULL && *head_node == NULL);

  // Find the HEAD element through document.getElementsByTagName.
  CComQIPtr<IHTMLDocument3> document3(document);
  CComPtr<IHTMLElementCollection> head_elements;
  DCHECK(document3 != NULL);  // Should be there on IE >= 5
  if (document3 == NULL) {
    LOG(ERROR) << L"Unable to retrieve IHTMLDocument3 interface";
    return E_NOINTERFACE;
  }
  HRESULT hr = GetElementsByTagName(document3, CComBSTR(L"head"),
                                    &head_elements, NULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "Could not retrieve head elements collection "
        << com::LogHr(hr);
    return hr;
  }

  return GetElementFromCollection(head_elements, 0, IID_IHTMLDOMNode,
                                  reinterpret_cast<void**>(head_node));
}

HRESULT DomUtils::GetElementsByTagName(IHTMLDocument3* document,
                                       BSTR tag_name,
                                       IHTMLElementCollection** elements,
                                       long* length) {
  DCHECK(document != NULL);
  DCHECK(tag_name != NULL);
  DCHECK(elements != NULL && *elements == NULL);

  HRESULT hr = document->getElementsByTagName(tag_name, elements);
  if (FAILED(hr) || *elements == NULL) {
    hr = com::AlwaysError(hr);
    LOG(ERROR) << "Could not retrieve elements collection " << com::LogHr(hr);
    return hr;
  }

  if (length != NULL) {
    hr = (*elements)->get_length(length);
    if (FAILED(hr)) {
      (*elements)->Release();
      *elements = NULL;
      LOG(ERROR) << "Could not retrieve collection length " << com::LogHr(hr);
    }
  }
  return hr;
}

HRESULT DomUtils::GetElementFromCollection(IHTMLElementCollection* collection,
                                           long index,
                                           REFIID id,
                                           void** element) {
  DCHECK(collection != NULL);
  DCHECK(element != NULL && *element == NULL);

  CComPtr<IDispatch> item;
  CComVariant index_variant(index, VT_I4);
  HRESULT hr = collection->item(index_variant, index_variant, &item);
  // As per http://msdn.microsoft.com/en-us/library/aa703930(VS.85).aspx
  // item may still be NULL even if S_OK is returned.
  if (FAILED(hr) || item == NULL) {
    hr = com::AlwaysError(hr);
    LOG(ERROR) << "Could not access item " << com::LogHr(hr);
    return hr;
  }

  return item->QueryInterface(id, element);
}
