// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BUGGY_BHO_HANDLING_H_
#define CHROME_FRAME_BUGGY_BHO_HANDLING_H_

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>

#include <vector>

#include "base/thread_local.h"

namespace buggy_bho {

// Method prototype for IDispatch::Invoke.
typedef HRESULT (__stdcall* InvokeFunc)(IDispatch* me, DISPID dispid,
                                        REFIID riid, LCID lcid, WORD flags,
                                        DISPPARAMS* params, VARIANT* result,
                                        EXCEPINFO* ei, UINT* err);

// Construct an instance of this class on the stack when firing web browser
// events that can be sent to buggy BHOs.  This class will intercept those
// BHOs (see list in cc file) and ignore notifications to those components
// for as long as the BuggyBhoTls instance on the stack lives.
class BuggyBhoTls {
 public:
  BuggyBhoTls();
  ~BuggyBhoTls();

  // Call after instantiating an instance of BuggyBhoTls.  This method traverses
  // the list of DWebBrowserEvents and DWebBrowserEvents2 subscribers and checks
  // if any of the sinks belong to a list of known-to-be-buggy BHOs.
  // For each of those, a patch will be applied that temporarily ignores certain
  // invokes.
  static HRESULT PatchBuggyBHOs(IWebBrowser2* browser);

 protected:
  // internal implementation:

  // Called when a buggy instance is found to be subscribing to browser events.
  void AddBuggyObject(IDispatch* obj);

  // Called from our patch to check if calls for this object should be ignored.
  // The reason we do this check is because there might be more than one browser
  // object running on the same thread (e.g. IE6) with one running CF and the
  // other MSHTML.  We don't want to drop events being fired by MSHTML, only
  // events fired by CF since these BHOs should handle MSHTML correctly.
  bool IsBuggyObject(IDispatch* obj) const;

  // Static, protected member methods

  // Returns the currently registered (TLS) BuggyBhoTls instance or NULL.
  static BuggyBhoTls* FromCurrentThread();

  // Patches a subscriber if it belongs to a buggy dll.
  static bool PatchIfBuggy(CONNECTDATA* cd, const IID& diid);

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

  // Pointer to a previous instance of BuggyBhoTls on this thread if any.
  // Under regular circumstances, this will be NULL.  However, there's a chance
  // that we could get reentrant calls, hence we maintain a stack.
  BuggyBhoTls* previous_instance_;

  // Where we store the current thread's instance.
  static base::ThreadLocalPointer<BuggyBhoTls> s_bad_object_tls_;
};

}  // end namespace buggy_bho

#endif  // CHROME_FRAME_BUGGY_BHO_HANDLING_H_
