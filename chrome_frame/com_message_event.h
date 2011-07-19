// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_COM_MESSAGE_EVENT_H_
#define CHROME_FRAME_COM_MESSAGE_EVENT_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>  // IHTMLEventObj

#include "base/basictypes.h"
#include "base/win/scoped_comptr.h"

// Implements a MessageEvent compliant event object by providing MessageEvent
// specific properties itself and inherited properties from a browser provided
// event implementation.
// NOTE: The messagePort and source properties will always be NULL.
// See the HTML 5 spec for further details on a MessageEvent object:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html#messageevent
class ComMessageEvent
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IDispatch {
 public:
  ComMessageEvent();
  ~ComMessageEvent();

BEGIN_COM_MAP(ComMessageEvent)
  COM_INTERFACE_ENTRY(IDispatch)
END_COM_MAP()

  // The dispids we support.  These are based on HTML5 and not IHTMLEventObj5
  // (there are a couple of differences).
  // http://dev.w3.org/html5/spec/Overview.html#messageevent
  // vs http://msdn.microsoft.com/en-us/library/cc288548(VS.85).aspx
  typedef enum {
    DISPID_MESSAGE_EVENT_DATA = 201,
    DISPID_MESSAGE_EVENT_ORIGIN,
    DISPID_MESSAGE_EVENT_LAST_EVENT_ID,
    DISPID_MESSAGE_EVENT_SOURCE,
    DISPID_MESSAGE_EVENT_MESSAGE_PORT,
    DISPID_MESSAGE_EVENT_TYPE
  } MessageEventDispIds;

  // Utility function for checking Invoke flags and assigning a BSTR to the
  // result variable.
  HRESULT GetStringProperty(WORD flags, const wchar_t* value, VARIANT* result);

  STDMETHOD(GetTypeInfoCount)(UINT* info);
  STDMETHOD(GetTypeInfo)(UINT which_info, LCID lcid, ITypeInfo** type_info);
  STDMETHOD(GetIDsOfNames)(REFIID iid, LPOLESTR* names, UINT count_names,
                           LCID lcid, DISPID* dispids);
  STDMETHOD(Invoke)(DISPID dispid, REFIID iid, LCID lcid, WORD flags,
                    DISPPARAMS* params, VARIANT* result, EXCEPINFO* excepinfo,
                    UINT* arg_err);

  // Initializes this object.  The container pointer is used to find the
  // container's IHTMLEventObj implementation if available.
  bool Initialize(IOleContainer* container, const std::string& message,
                  const std::string& origin, const std::string& event_type);

 protected:
  // HTML Event object to which we delegate property and method
  // calls that we do not directly support.  This may not be initialized
  // if our container does not require the properties/methods exposed by
  // the basic event object.
  base::win::ScopedComPtr<IHTMLEventObj> basic_event_;
  std::string message_;
  std::string origin_;
  std::string type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComMessageEvent);
};

#endif  // CHROME_FRAME_COM_MESSAGE_EVENT_H_
