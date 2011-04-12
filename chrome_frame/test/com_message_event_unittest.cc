// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/com_message_event.h"

#include <atlbase.h>
#include <atlcom.h>

#include "testing/gtest/include/gtest/gtest.h"

// To allow the unit test read-only access to check protected member variables.
class FriendlyComMessageEvent : public ComMessageEvent {
 public:
  inline IHTMLEventObj* basic_event() { return basic_event_; }
};

class ATL_NO_VTABLE MockDumbContainer :
    public CComObjectRoot,
    public IOleContainer {
 public:
  DECLARE_NOT_AGGREGATABLE(MockDumbContainer)
  BEGIN_COM_MAP(MockDumbContainer)
    COM_INTERFACE_ENTRY(IParseDisplayName)
    COM_INTERFACE_ENTRY(IOleContainer)
  END_COM_MAP()

  STDMETHOD(ParseDisplayName)(IBindCtx*, LPOLESTR, ULONG*, IMoniker**) {
    NOTREACHED();
    return E_NOTIMPL;
  }
  STDMETHOD(EnumObjects)(DWORD, IEnumUnknown**) {
    NOTREACHED();
    return E_NOTIMPL;
  }
  STDMETHOD(LockContainer)(BOOL) {
    NOTREACHED();
    return E_NOTIMPL;
  }
};

TEST(ComMessageEvent, WithDumbContainer) {
  CComObject<MockDumbContainer>* container_obj = NULL;
  CComObject<MockDumbContainer>::CreateInstance(&container_obj);
  base::win::ScopedComPtr<IOleContainer> container(container_obj);
  EXPECT_FALSE(!container);

  CComObject<FriendlyComMessageEvent>* event_obj = NULL;
  CComObject<FriendlyComMessageEvent>::CreateInstance(&event_obj);
  base::win::ScopedComPtr<IUnknown> event_ref(event_obj);

  bool result = event_obj->Initialize(container, "hi", "http://www.foo.com/",
                                      "message");
  EXPECT_TRUE(result);
  EXPECT_TRUE(!event_obj->basic_event());
}

// Mock object to mimic a "smart" container, e.g. IE, that will
// be able to return an IHTMLDocument2 and 4, and from which you
// can get an IHTMLEventObj implementation.  Doubles as a mock
// IHTMLEventObj implementation.
class ATL_NO_VTABLE MockSmartContainer :
    public CComObjectRoot,
    public IOleContainer,
    public IHTMLDocument2,
    public IHTMLDocument4,
    public IHTMLEventObj {
 public:
  DECLARE_NOT_AGGREGATABLE(MockSmartContainer)
  BEGIN_COM_MAP(MockSmartContainer)
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLEventObj)
    COM_INTERFACE_ENTRY(IParseDisplayName)
    COM_INTERFACE_ENTRY(IOleContainer)
    COM_INTERFACE_ENTRY(IHTMLDocument)
    COM_INTERFACE_ENTRY(IHTMLDocument2)
    COM_INTERFACE_ENTRY(IHTMLDocument4)
    COM_INTERFACE_ENTRY(IHTMLEventObj)
  END_COM_MAP()

  static const DISPID kDispId = 424242;
  static const long kResultValue = 42;

  // Only method we actually implement from IHTMLDocument4, to give
  // out the mock IHTMLEventObj.
  STDMETHOD(createEventObject)(VARIANT*, IHTMLEventObj** event_obj) {
    return GetUnknown()->QueryInterface(event_obj);
  }

  // Dummy IDispatch implementation for unit testing, to validate
  // passthrough semantics.
  STDMETHOD(GetIDsOfNames)(REFIID iid, LPOLESTR* names, UINT num_names, 
                           LCID lcid, DISPID* disp_ids) {
    DCHECK(num_names == 1);
    disp_ids[0] = kDispId;
    return S_OK;
  }
  
  STDMETHOD(Invoke)(DISPID id, REFIID iid, LCID lcid, WORD flags, 
                    DISPPARAMS* disp_params, VARIANT* var_result, 
                    EXCEPINFO* excep_info, UINT* arg_error) {
    var_result->vt = VT_I4;
    var_result->lVal = kResultValue;
    return S_OK;
  }
  

  // Do-nothing implementation of the rest of the interface methods.
  // To make this less verbose, define a macro here and undefine it
  // at the end of the list.
#define STDMETHODNOTIMP(method, parameters) \
    STDMETHOD(method) parameters { \
      NOTREACHED(); \
      return E_NOTIMPL; \
    }

  // IDispatch
  STDMETHODNOTIMP(GetTypeInfoCount, (UINT*));
  STDMETHODNOTIMP(GetTypeInfo, (UINT, LCID, ITypeInfo**));

  // IParseDisplayName
  STDMETHODNOTIMP(ParseDisplayName, (IBindCtx*, LPOLESTR, ULONG*, IMoniker**));
  // IOleContainer
  STDMETHODNOTIMP(EnumObjects, (DWORD, IEnumUnknown**));
  STDMETHODNOTIMP(LockContainer, (BOOL));
  // IHTMLDocument
  STDMETHODNOTIMP(get_Script, (IDispatch**));
  // IHTMLDocument2
  STDMETHODNOTIMP(get_all, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_body, (IHTMLElement**));
  STDMETHODNOTIMP(get_activeElement, (IHTMLElement**));
  STDMETHODNOTIMP(get_images, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_applets, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_links, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_forms, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_anchors, (IHTMLElementCollection**));
  STDMETHODNOTIMP(put_title, (BSTR));
  STDMETHODNOTIMP(get_title, (BSTR*));
  STDMETHODNOTIMP(get_scripts, (IHTMLElementCollection**));
  STDMETHODNOTIMP(put_designMode, (BSTR));
  STDMETHODNOTIMP(get_designMode, (BSTR*));
  STDMETHODNOTIMP(get_selection, (IHTMLSelectionObject**));
  STDMETHODNOTIMP(get_readyState, (BSTR*));
  STDMETHODNOTIMP(get_frames, (IHTMLFramesCollection2**));
  STDMETHODNOTIMP(get_embeds, (IHTMLElementCollection**));
  STDMETHODNOTIMP(get_plugins, (IHTMLElementCollection**));
  STDMETHODNOTIMP(put_alinkColor, (VARIANT));
  STDMETHODNOTIMP(get_alinkColor, (VARIANT*));
  STDMETHODNOTIMP(put_bgColor, (VARIANT));
  STDMETHODNOTIMP(get_bgColor, (VARIANT*));
  STDMETHODNOTIMP(put_fgColor, (VARIANT));
  STDMETHODNOTIMP(get_fgColor, (VARIANT*));
  STDMETHODNOTIMP(put_linkColor, (VARIANT));
  STDMETHODNOTIMP(get_linkColor, (VARIANT*));
  STDMETHODNOTIMP(put_vlinkColor, (VARIANT));
  STDMETHODNOTIMP(get_vlinkColor, (VARIANT*));
  STDMETHODNOTIMP(get_referrer, (BSTR*));
  STDMETHODNOTIMP(get_location, (IHTMLLocation**));
  STDMETHODNOTIMP(get_lastModified, (BSTR*));
  STDMETHODNOTIMP(put_URL, (BSTR));
  STDMETHODNOTIMP(get_URL, (BSTR*));
  STDMETHODNOTIMP(put_domain, (BSTR));
  STDMETHODNOTIMP(get_domain, (BSTR*));
  STDMETHODNOTIMP(put_cookie, (BSTR));
  STDMETHODNOTIMP(get_cookie, (BSTR*));
  STDMETHODNOTIMP(put_expando, (VARIANT_BOOL));
  STDMETHODNOTIMP(get_expando, (VARIANT_BOOL*));
  STDMETHODNOTIMP(put_charset, (BSTR));
  STDMETHODNOTIMP(get_charset, (BSTR*));
  STDMETHODNOTIMP(put_defaultCharset, (BSTR));
  STDMETHODNOTIMP(get_defaultCharset, (BSTR*));
  STDMETHODNOTIMP(get_mimeType, (BSTR*));
  STDMETHODNOTIMP(get_fileSize, (BSTR*));
  STDMETHODNOTIMP(get_fileCreatedDate, (BSTR*));
  STDMETHODNOTIMP(get_fileModifiedDate, (BSTR*));
  STDMETHODNOTIMP(get_fileUpdatedDate, (BSTR*));
  STDMETHODNOTIMP(get_security, (BSTR*));
  STDMETHODNOTIMP(get_protocol, (BSTR*));
  STDMETHODNOTIMP(get_nameProp, (BSTR*));
  STDMETHODNOTIMP(write, (SAFEARRAY*));
  STDMETHODNOTIMP(writeln, (SAFEARRAY*));
  STDMETHODNOTIMP(open, (BSTR, VARIANT, VARIANT, VARIANT, IDispatch**));
  STDMETHODNOTIMP(close, ());
  STDMETHODNOTIMP(clear, ());
  STDMETHODNOTIMP(queryCommandSupported, (BSTR, VARIANT_BOOL*));
  STDMETHODNOTIMP(queryCommandEnabled, (BSTR, VARIANT_BOOL*));
  STDMETHODNOTIMP(queryCommandState, (BSTR, VARIANT_BOOL*));
  STDMETHODNOTIMP(queryCommandIndeterm, (BSTR, VARIANT_BOOL*));
  STDMETHODNOTIMP(queryCommandText, (BSTR, BSTR*));
  STDMETHODNOTIMP(queryCommandValue, (BSTR, VARIANT*));
  STDMETHODNOTIMP(execCommand, (BSTR, VARIANT_BOOL, VARIANT, VARIANT_BOOL*));
  STDMETHODNOTIMP(execCommandShowHelp, (BSTR, VARIANT_BOOL*));
  STDMETHODNOTIMP(createElement, (BSTR, IHTMLElement**));
  STDMETHODNOTIMP(put_onhelp, (VARIANT));
  STDMETHODNOTIMP(get_onhelp, (VARIANT*));
  STDMETHODNOTIMP(put_onclick, (VARIANT));
  STDMETHODNOTIMP(get_onclick, (VARIANT*));
  STDMETHODNOTIMP(put_ondblclick, (VARIANT));
  STDMETHODNOTIMP(get_ondblclick, (VARIANT*));
  STDMETHODNOTIMP(put_onkeyup, (VARIANT));
  STDMETHODNOTIMP(get_onkeyup, (VARIANT*));
  STDMETHODNOTIMP(put_onkeydown, (VARIANT));
  STDMETHODNOTIMP(get_onkeydown, (VARIANT*));
  STDMETHODNOTIMP(put_onkeypress, (VARIANT));
  STDMETHODNOTIMP(get_onkeypress, (VARIANT*));
  STDMETHODNOTIMP(put_onmouseup, (VARIANT));
  STDMETHODNOTIMP(get_onmouseup, (VARIANT*));
  STDMETHODNOTIMP(put_onmousedown, (VARIANT));
  STDMETHODNOTIMP(get_onmousedown, (VARIANT*));
  STDMETHODNOTIMP(put_onmousemove, (VARIANT));
  STDMETHODNOTIMP(get_onmousemove, (VARIANT*));
  STDMETHODNOTIMP(put_onmouseout, (VARIANT));
  STDMETHODNOTIMP(get_onmouseout, (VARIANT*));
  STDMETHODNOTIMP(put_onmouseover, (VARIANT));
  STDMETHODNOTIMP(get_onmouseover, (VARIANT*));
  STDMETHODNOTIMP(put_onreadystatechange, (VARIANT));
  STDMETHODNOTIMP(get_onreadystatechange, (VARIANT*));
  STDMETHODNOTIMP(put_onafterupdate, (VARIANT));
  STDMETHODNOTIMP(get_onafterupdate, (VARIANT*));
  STDMETHODNOTIMP(put_onrowexit, (VARIANT));
  STDMETHODNOTIMP(get_onrowexit, (VARIANT*));
  STDMETHODNOTIMP(put_onrowenter, (VARIANT));
  STDMETHODNOTIMP(get_onrowenter, (VARIANT*));
  STDMETHODNOTIMP(put_ondragstart, (VARIANT));
  STDMETHODNOTIMP(get_ondragstart, (VARIANT*));
  STDMETHODNOTIMP(put_onselectstart, (VARIANT));
  STDMETHODNOTIMP(get_onselectstart, (VARIANT*));
  STDMETHODNOTIMP(elementFromPoint, (long, long, IHTMLElement**));
  STDMETHODNOTIMP(get_parentWindow, (IHTMLWindow2**));
  STDMETHODNOTIMP(get_styleSheets, (IHTMLStyleSheetsCollection**));
  STDMETHODNOTIMP(put_onbeforeupdate, (VARIANT));
  STDMETHODNOTIMP(get_onbeforeupdate, (VARIANT*));
  STDMETHODNOTIMP(put_onerrorupdate, (VARIANT));
  STDMETHODNOTIMP(get_onerrorupdate, (VARIANT*));
  STDMETHODNOTIMP(toString, (BSTR*));
  STDMETHODNOTIMP(createStyleSheet, (BSTR, long, IHTMLStyleSheet**));
  // IHTMLDocument4
  STDMETHODNOTIMP(focus, ());
  STDMETHODNOTIMP(hasFocus, (VARIANT_BOOL*));
  STDMETHODNOTIMP(put_onselectionchange, (VARIANT));
  STDMETHODNOTIMP(get_onselectionchange, (VARIANT*));
  STDMETHODNOTIMP(get_namespaces, (IDispatch**));
  STDMETHODNOTIMP(createDocumentFromUrl, (BSTR, BSTR, IHTMLDocument2**));
  STDMETHODNOTIMP(put_media, (BSTR));
  STDMETHODNOTIMP(get_media, (BSTR*));
  STDMETHODNOTIMP(fireEvent, (BSTR, VARIANT*, VARIANT_BOOL*));
  STDMETHODNOTIMP(createRenderStyle, (BSTR, IHTMLRenderStyle**));
  STDMETHODNOTIMP(put_oncontrolselect, (VARIANT));
  STDMETHODNOTIMP(get_oncontrolselect, (VARIANT*));
  STDMETHODNOTIMP(get_URLUnencoded, (BSTR*));
  // IHTMLEventObj
  STDMETHODNOTIMP(get_srcElement, (IHTMLElement**))
  STDMETHODNOTIMP(get_altKey, (VARIANT_BOOL*));
  STDMETHODNOTIMP(get_ctrlKey, (VARIANT_BOOL*));
  STDMETHODNOTIMP(get_shiftKey, (VARIANT_BOOL*));
  STDMETHODNOTIMP(put_returnValue, (VARIANT));
  STDMETHODNOTIMP(get_returnValue, (VARIANT*));
  STDMETHODNOTIMP(put_cancelBubble, (VARIANT_BOOL));
  STDMETHODNOTIMP(get_cancelBubble, (VARIANT_BOOL*));
  STDMETHODNOTIMP(get_fromElement, (IHTMLElement**));
  STDMETHODNOTIMP(get_toElement, (IHTMLElement**));
  STDMETHODNOTIMP(put_keyCode, (long));
  STDMETHODNOTIMP(get_keyCode, (long*));
  STDMETHODNOTIMP(get_button, (long*));
  STDMETHODNOTIMP(get_type, (BSTR*));
  STDMETHODNOTIMP(get_qualifier, (BSTR*));
  STDMETHODNOTIMP(get_reason, (long*));
  STDMETHODNOTIMP(get_x, (long*));
  STDMETHODNOTIMP(get_y, (long*));
  STDMETHODNOTIMP(get_clientX, (long*));
  STDMETHODNOTIMP(get_clientY, (long*));
  STDMETHODNOTIMP(get_offsetX, (long*));
  STDMETHODNOTIMP(get_offsetY, (long*));
  STDMETHODNOTIMP(get_screenX, (long*));
  STDMETHODNOTIMP(get_screenY, (long*));
  STDMETHODNOTIMP(get_srcFilter, (IDispatch**));
#undef STDMETHODNOTIMP
};

TEST(ComMessageEvent, WithSmartContainer) {
  CComObject<MockSmartContainer>* container_obj = NULL;
  CComObject<MockSmartContainer>::CreateInstance(&container_obj);
  base::win::ScopedComPtr<IOleContainer> container(container_obj);
  EXPECT_FALSE(!container);

  CComObject<FriendlyComMessageEvent>* event_obj = NULL;
  CComObject<FriendlyComMessageEvent>::CreateInstance(&event_obj);
  base::win::ScopedComPtr<IUnknown> event_ref(event_obj);

  bool succeeded = event_obj->Initialize(container, "hi",
                                         "http://www.foo.com/", "message");
  EXPECT_TRUE(succeeded);
  EXPECT_FALSE(!event_obj->basic_event());

  // Name handled natively by CF's ComMessageEvent.
  DISPID dispid = -1;
  LPOLESTR name = L"data";
  HRESULT hr = event_obj->GetIDsOfNames(IID_IDispatch, &name, 1,
                                        LOCALE_USER_DEFAULT, &dispid);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(dispid, ComMessageEvent::DISPID_MESSAGE_EVENT_DATA);

  // Name not handled by CF's ComMessageEvent.
  dispid = -1;
  name = L"nothandledatallbyanyone";
  hr = event_obj->GetIDsOfNames(IID_IDispatch, &name, 1,
                                LOCALE_USER_DEFAULT, &dispid);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(dispid, MockSmartContainer::kDispId);

  // Invoke function handled by ComMessageEvent.
  CComDispatchDriver dispatcher(event_obj);
  CComVariant result;
  hr = dispatcher.GetProperty(ComMessageEvent::DISPID_MESSAGE_EVENT_DATA,
                              &result);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(result.vt, VT_BSTR);
  EXPECT_EQ(wcscmp(result.bstrVal, L"hi"), 0);

  // And now check passthrough.
  result.Clear();
  hr = dispatcher.GetProperty(MockSmartContainer::kDispId, &result);
  EXPECT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(result.vt, VT_I4);
  EXPECT_EQ(result.lVal, MockSmartContainer::kResultValue);
}
