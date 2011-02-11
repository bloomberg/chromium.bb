// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BUGGY_BHO_HANDLING_H_
#define CHROME_FRAME_BUGGY_BHO_HANDLING_H_

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>

#include <vector>

#include "base/threading/thread_local.h"
#include "base/win/scoped_comptr.h"

namespace buggy_bho {

// Method prototype for IDispatch::Invoke.
typedef HRESULT (__stdcall* InvokeFunc)(IDispatch* me, DISPID dispid,
                                        REFIID riid, LCID lcid, WORD flags,
                                        DISPPARAMS* params, VARIANT* result,
                                        EXCEPINFO* ei, UINT* err);

// Construct a per thread instance of this class before firing web browser
// events that can be sent to buggy BHOs.  This class will intercept those
// BHOs (see list in cc file) and ignore notifications to those components
// as long as ChromeFrame is the active view.
class BuggyBhoTls {
 public:
  // This method traverses the list of DWebBrowserEvents and DWebBrowserEvents2
  // subscribers and checks if any of the sinks belong to a list of
  // known-to-be-buggy BHOs.
  // For each of those, a patch will be applied that temporarily ignores certain
  // invokes.
  HRESULT PatchBuggyBHOs(IWebBrowser2* browser);

  // Returns the instance of the BuggyBhoTls object for the current thread.
  static BuggyBhoTls* GetInstance();

  // Destroys the BuggyBhoTls instance foe the current thread.
  static void DestroyInstance();

  void set_web_browser(IWebBrowser2* web_browser2) {
    web_browser2_ = web_browser2;
  }

 protected:
  BuggyBhoTls();
  ~BuggyBhoTls();
  // internal implementation:

  // Called when a buggy instance is found to be subscribing to browser events.
  void AddBuggyObject(IDispatch* obj);

  // Called from our patch to check if calls for this object should be ignored.
  // The reason we do this check is because there might be more than one browser
  // object running on the same thread (e.g. IE6) with one running CF and the
  // other MSHTML.  We don't want to drop events being fired by MSHTML, only
  // events fired by CF since these BHOs should handle MSHTML correctly.
  bool ShouldSkipInvoke(IDispatch* obj) const;

  // Static, protected member methods

  // Patches a subscriber if it belongs to a buggy dll.
  bool PatchIfBuggy(IUnknown* unk, const IID& diid);

  // Patches the IDispatch::Invoke method.
  static HRESULT PatchInvokeMethod(PROC* invoke);

  // This is our substitute function that is called instead of the buggy DLLs.
  // Here we call IsBuggyObject to check if we should ignore invokes or allow
  // them to go through.
  static STDMETHODIMP BuggyBhoInvoke(InvokeFunc original, IDispatch* me,
      DISPID dispid, REFIID riid, LCID lcid, WORD flags, DISPPARAMS* params,
      VARIANT* result, EXCEPINFO* ei, UINT* err);

 protected:
  // List of buggy subscribers.
  std::vector<IDispatch*> bad_objects_;
  // Where we store the current thread's instance.
  static base::ThreadLocalPointer<BuggyBhoTls> s_bad_object_tls_;
  // The IWebBrowser2 instance for this thread.
  base::win::ScopedComPtr<IWebBrowser2> web_browser2_;
  // Set to true when we are done patching the event sinks of buggy bho's.
  bool patched_;
};

}  // end namespace buggy_bho

#endif  // CHROME_FRAME_BUGGY_BHO_HANDLING_H_
