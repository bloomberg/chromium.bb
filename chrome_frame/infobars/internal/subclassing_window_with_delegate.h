// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_INFOBARS_INTERNAL_SUBCLASSING_WINDOW_WITH_DELEGATE_H_
#define CHROME_FRAME_INFOBARS_INTERNAL_SUBCLASSING_WINDOW_WITH_DELEGATE_H_

#include <atlbase.h>
#include <atlcrack.h>
#include <atlwin.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome_frame/pin_module.h"

// Implements behavior common to HostWindowManager and DisplacedWindowManager.
template<typename T> class SubclassingWindowWithDelegate
    : public CWindowImpl<T> {
 public:
  // Allows clients to modify the dimensions of the displaced window.
  // Through its destructor, allows clients to know when the subclassed window
  // is destroyed.
  class Delegate {
   public:
    // The delegate will be deleted when the subclassed window is destroyed.
    virtual ~Delegate() {}

    // Receives the natural dimensions of the displaced window. Upon return,
    // rect should contain the adjusted dimensions (i.e., possibly reduced to
    // accomodate an infobar).
    virtual void AdjustDisplacedWindowDimensions(RECT* rect) = 0;
  };  // class Delegate

  SubclassingWindowWithDelegate() {}

  // Returns true if the window is successfully subclassed, in which case this
  // instance will take responsibility for its own destruction when the window
  // is destroyed. If this method returns false, the caller should delete the
  // instance immediately.
  //
  // Takes ownership of delegate in either case, deleting it when the window
  // is destroyed (or immediately, in case of failure).
  bool Initialize(HWND hwnd, Delegate* delegate) {
    DCHECK(delegate != NULL);
    DCHECK(delegate_ == NULL);
    scoped_ptr<Delegate> new_delegate(delegate);

    if (!::IsWindow(hwnd) || !SubclassWindow(hwnd)) {
      PLOG(ERROR) << "Failed to subclass an HWND";
      return false;
    }

    // Ensure we won't be unloaded while our window proc is attached to the tab
    // window.
    chrome_frame::PinModule();

    delegate_.swap(new_delegate);

    return true;
  }

  // Returns the delegate associated with the specified window, if any.
  static Delegate* GetDelegateForHwnd(HWND hwnd) {
    return reinterpret_cast<Delegate*>(
        ::SendMessage(hwnd, RegisterGetDelegateMessage(), NULL, NULL));
  }

  BEGIN_MSG_MAP_EX(SubclassingWindowWithDelegate)
    MESSAGE_HANDLER(RegisterGetDelegateMessage(), OnGetDelegate)
    MSG_WM_DESTROY(OnDestroy)
  END_MSG_MAP()

  // This instance is now free to be deleted.
  virtual void OnFinalMessage(HWND hwnd) {
    delete this;
  }

 protected:
  scoped_ptr<Delegate>& delegate() { return delegate_; }

 private:
  // Registers a unique ID for our custom event.
  static UINT RegisterGetDelegateMessage() {
    static UINT message_id(
        RegisterWindowMessage(L"SubclassingWindowWithDelegate::OnGetDelegate"));
    return message_id;
  }

  // The subclassed window has been destroyed. Delete the delegate. We will
  // delete ourselves in OnFinalMessage.
  void OnDestroy() {
    delegate_.reset();
  }

  LRESULT OnGetDelegate(UINT message,
                        WPARAM wparam,
                        LPARAM lparam,
                        BOOL& handled) {
    return reinterpret_cast<LRESULT>(delegate_.get());
  }

  scoped_ptr<Delegate> delegate_;
  DISALLOW_COPY_AND_ASSIGN(SubclassingWindowWithDelegate);
};  // class SubclassingWindowWithDelegate

#endif  // CHROME_FRAME_INFOBARS_INTERNAL_SUBCLASSING_WINDOW_WITH_DELEGATE_H_
