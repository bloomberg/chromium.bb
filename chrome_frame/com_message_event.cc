// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/com_message_event.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

ComMessageEvent::ComMessageEvent() {
}

ComMessageEvent::~ComMessageEvent() {
}

bool ComMessageEvent::Initialize(IOleContainer* container,
                                 const std::string& message,
                                 const std::string& origin,
                                 const std::string& event_type) {
  DLOG_IF(WARNING, !container) << __FUNCTION__ << " no container";
  message_ = message;
  origin_ = origin;
  type_ = event_type;

  // May remain NULL if container not IE
  ScopedComPtr<IHTMLEventObj> basic_event;
  ScopedComPtr<IHTMLDocument2> doc;

  // Fetching doc may fail in non-IE containers
  // and container might be NULL in some applications.
  if (container)
    container->QueryInterface(doc.Receive());
  if (doc) {
    ScopedComPtr<IHTMLDocument4> doc4;
    doc4.QueryFrom(doc);
    DCHECK(doc4);  // supported by IE5.5 and higher
    if (doc4) {
      // IHTMLEventObj5 is only supported in IE8 and later, so we provide our
      // own (minimalistic) implementation of it.
      doc4->createEventObject(NULL, basic_event.Receive());
      DCHECK(basic_event);
    }
  }

  basic_event_ = basic_event;
  return true;
}

STDMETHODIMP ComMessageEvent::GetTypeInfoCount(UINT* info) {
  // Don't DCHECK as python scripts might still call this function
  // inadvertently.
  DLOG(WARNING) << "Not implemented: " << __FUNCTION__;
  return E_NOTIMPL;
}

STDMETHODIMP ComMessageEvent::GetTypeInfo(UINT which_info, LCID lcid,
                                          ITypeInfo** type_info) {
  DLOG(WARNING) << "Not implemented: " << __FUNCTION__;
  return E_NOTIMPL;
}

STDMETHODIMP ComMessageEvent::GetIDsOfNames(REFIID iid, LPOLESTR* names,
                                            UINT count_names, LCID lcid,
                                            DISPID* dispids) {
  HRESULT hr = S_OK;

  // Note that since we're using LowerCaseEqualsASCII for string comparison,
  // the second argument _must_ be all lower case.  I.e. we cannot compare
  // against L"messagePort" since it has a capital 'P'.
  for (UINT i = 0; SUCCEEDED(hr) && i < count_names; ++i) {
    const wchar_t* name_begin = names[i];
    const wchar_t* name_end = name_begin + wcslen(name_begin);
    if (LowerCaseEqualsASCII(name_begin, name_end, "data")) {
      dispids[i] = DISPID_MESSAGE_EVENT_DATA;
    } else if (LowerCaseEqualsASCII(name_begin, name_end, "origin")) {
      dispids[i] = DISPID_MESSAGE_EVENT_ORIGIN;
    } else if (LowerCaseEqualsASCII(name_begin, name_end, "lasteventid")) {
      dispids[i] = DISPID_MESSAGE_EVENT_LAST_EVENT_ID;
    } else if (LowerCaseEqualsASCII(name_begin, name_end, "source")) {
      dispids[i] = DISPID_MESSAGE_EVENT_SOURCE;
    } else if (LowerCaseEqualsASCII(name_begin, name_end, "messageport")) {
      dispids[i] = DISPID_MESSAGE_EVENT_MESSAGE_PORT;
    } else if (LowerCaseEqualsASCII(name_begin, name_end, "type")) {
      dispids[i] = DISPID_MESSAGE_EVENT_TYPE;
    } else {
      if (basic_event_) {
        hr = basic_event_->GetIDsOfNames(IID_IDispatch, &names[i], 1, lcid,
                                         &dispids[i]);
      } else {
        hr = DISP_E_MEMBERNOTFOUND;
      }

      if (FAILED(hr)) {
        DLOG(WARNING) << "member not found: " << names[i]
                      << base::StringPrintf(L"0x%08X", hr);
      }
    }
  }
  return hr;
}

STDMETHODIMP ComMessageEvent::Invoke(DISPID dispid, REFIID iid, LCID lcid,
                                     WORD flags, DISPPARAMS* params,
                                     VARIANT* result, EXCEPINFO* excepinfo,
                                     UINT* arg_err) {
  HRESULT hr = DISP_E_MEMBERNOTFOUND;
  switch (dispid) {
    case DISPID_MESSAGE_EVENT_DATA:
      hr = GetStringProperty(flags, UTF8ToWide(message_).c_str(), result);
      break;

    case DISPID_MESSAGE_EVENT_ORIGIN:
      hr = GetStringProperty(flags, UTF8ToWide(origin_).c_str(), result);
      break;

    case DISPID_MESSAGE_EVENT_TYPE:
      hr = GetStringProperty(flags, UTF8ToWide(type_).c_str(), result);
      break;

    case DISPID_MESSAGE_EVENT_LAST_EVENT_ID:
      hr = GetStringProperty(flags, L"", result);
      break;

    case DISPID_MESSAGE_EVENT_SOURCE:
    case DISPID_MESSAGE_EVENT_MESSAGE_PORT:
      if (flags & DISPATCH_PROPERTYGET) {
        result->vt = VT_NULL;
        hr = S_OK;
      } else {
        hr = DISP_E_TYPEMISMATCH;
      }
      break;

    default:
      if (basic_event_) {
        hr = basic_event_->Invoke(dispid, iid, lcid, flags, params, result,
                                  excepinfo, arg_err);
      }
      break;
  }

  return hr;
}

HRESULT ComMessageEvent::GetStringProperty(WORD flags, const wchar_t* value,
                                           VARIANT* result) {
  if (!result)
    return E_INVALIDARG;

  HRESULT hr;
  if (flags & DISPATCH_PROPERTYGET) {
    result->vt = VT_BSTR;
    result->bstrVal = ::SysAllocString(value);
    hr = S_OK;
  } else {
    hr = DISP_E_TYPEMISMATCH;
  }
  return hr;
}
