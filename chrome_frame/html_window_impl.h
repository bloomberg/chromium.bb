// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_HTML_WINDOW_IMPL_H_
#define CHROME_FRAME_HTML_WINDOW_IMPL_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>

#include "chrome_tab.h"  // NOLINT
#include "chrome_frame/resource.h"
#include "grit/chrome_frame_resources.h"

template <typename T>
class ATL_NO_VTABLE HTMLWindowImpl
  : public IDispatchImpl<T> {
 public:
  HTMLWindowImpl() {}

  // IHTMLFramesCollection2
  STDMETHOD(item)(VARIANT* index, VARIANT* result) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_length)(long* length) {
    DLOG(INFO) << __FUNCTION__;
    if (!length)
      return E_POINTER;

    *length = 0;
    return S_OK;
  }

  // IHTMLWindow2
  STDMETHOD(get_frames)(IHTMLFramesCollection2** collection) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_defaultStatus)(BSTR status) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_defaultStatus)(BSTR* status) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_status)(BSTR status) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_status)(BSTR* status) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(setTimeout)(BSTR expression, long msec, VARIANT* language,
                        long* timer_id) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(clearTimeout)(long timer_id) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(alert)(BSTR message) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(confirm)(BSTR message, VARIANT_BOOL* confirmed) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(prompt)(BSTR message, BSTR defstr, VARIANT* textdata) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_Image)(IHTMLImageElementFactory** factory) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_location)(IHTMLLocation** location) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_history)(IOmHistory** history) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(close)() {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_opener)(VARIANT opener) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_opener)(VARIANT* opener) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_navigator)(IOmNavigator** navigator) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_name)(BSTR name) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_name)(BSTR* name) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_parent)(IHTMLWindow2** parent) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(open)(BSTR url, BSTR name, BSTR features, VARIANT_BOOL replace,
                  IHTMLWindow2** window_result) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_self)(IHTMLWindow2** self) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_top)(IHTMLWindow2** top) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_window)(IHTMLWindow2** window) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(navigate)(BSTR url) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onfocus)(VARIANT focus_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onfocus)(VARIANT* focus_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onblur)(VARIANT blur_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onblur)(VARIANT* blur_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onload)(VARIANT onload_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onload)(VARIANT* onload_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onbeforeunload)(VARIANT before_onload) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onbeforeunload)(VARIANT* before_onload) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onunload)(VARIANT unload_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onunload)(VARIANT* unload_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onhelp)(VARIANT help_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onhelp)(VARIANT* help_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onerror)(VARIANT error_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onerror)(VARIANT* error_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onresize)(VARIANT resize_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onresize)(VARIANT* resize_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_onscroll)(VARIANT scroll_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_onscroll)(VARIANT* scroll_handler) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_document)(IHTMLDocument2** document) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_event)(IHTMLEventObj** event_object) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get__newEnum)(IUnknown** new_enum) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(showModalDialog)(BSTR dialog, VARIANT* in, VARIANT* options,
            VARIANT* out) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(showHelp)(BSTR help_url, VARIANT help_arg, BSTR features) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_screen)(IHTMLScreen** screen) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_Option)(IHTMLOptionElementFactory** option_factory) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(focus)() {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_closed)(VARIANT_BOOL* is_closed) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(blur)() {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(scroll)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_clientInformation)(IOmNavigator** navigator) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(setInterval)(BSTR expression, long msec, VARIANT* language,
            long* timerID) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(clearInterval)(long timerID) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(put_offscreenBuffering)(VARIANT off_screen_buffering) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_offscreenBuffering)(VARIANT* off_screen_buffering) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(execScript)(BSTR code, BSTR language, VARIANT* ret) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(toString)(BSTR* String) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(scrollBy)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(scrollTo)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(moveTo)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(moveBy)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(resizeTo)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(resizeBy)(long x, long y) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(get_external)(IDispatch** external) {
    DLOG(INFO) << __FUNCTION__;
    return E_NOTIMPL;
  }

};

#endif  // CHROME_FRAME_HTML_WINDOW_IMPL_H_

