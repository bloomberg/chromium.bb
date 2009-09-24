// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_SCOPED_NS_PTR_WIN_H_
#define CHROME_FRAME_SCOPED_NS_PTR_WIN_H_

#include "base/logging.h"
#include "base/ref_counted.h"

#include "third_party/xulrunner-sdk/win/include/xpcom/nsISupports.h"


// Utility template to prevent users of ScopedNsPtr from calling AddRef and/or
// Release() without going through the ScopedNsPtr class.
template <class nsInterface>
class BlocknsISupportsMethods : public nsInterface {
 private:
  NS_IMETHOD QueryInterface(REFNSIID iid, void** object) = 0;
  NS_IMETHOD_(nsrefcnt) AddRef() = 0;
  NS_IMETHOD_(nsrefcnt) Release() = 0;
};

// A smart pointer class for nsISupports.
// Based on ScopedComPtr.
// We have our own class instead of nsCOMPtr.  nsCOMPtr has parts of its
// implementation in the xpcomglue lib which we can't use since that lib
// is built with incompatible build flags to ours.
template <class nsInterface,
          const nsIID* interface_id =
              reinterpret_cast<const nsIID*>(&__uuidof(nsInterface))>
class ScopedNsPtr : public scoped_refptr<nsInterface> {
 public:
  typedef scoped_refptr<nsInterface> ParentClass;

  ScopedNsPtr() {
  }

  explicit ScopedNsPtr(nsInterface* p) : ParentClass(p) {
  }

  explicit ScopedNsPtr(const ScopedNsPtr<nsInterface, interface_id>& p)
      : ParentClass(p) {
  }

  ~ScopedNsPtr() {
    // We don't want the smart pointer class to be bigger than the pointer
    // it wraps.
    COMPILE_ASSERT(sizeof(ScopedNsPtr<nsInterface, interface_id>) ==
                   sizeof(nsInterface*), ScopedNsPtrSize);
  }

  // Explicit Release() of the held object.  Useful for reuse of the
  // ScopedNsPtr instance.
  // Note that this function equates to nsISupports::Release and should not
  // be confused with e.g. scoped_ptr::release().
  void Release() {
    if (ptr_ != NULL) {
      ptr_->Release();
      ptr_ = NULL;
    }
  }

  // Sets the internal pointer to NULL and returns the held object without
  // releasing the reference.
  nsInterface* Detach() {
    nsInterface* p = ptr_;
    ptr_ = NULL;
    return p;
  }

  // Accepts an interface pointer that has already been addref-ed.
  void Attach(nsInterface* p) {
    DCHECK(ptr_ == NULL);
    ptr_ = p;
  }

  // Retrieves the pointer address.
  // Used to receive object pointers as out arguments (and take ownership).
  // The function DCHECKs on the current value being NULL.
  // Usage: Foo(p.Receive());
  nsInterface** Receive() {
    DCHECK(ptr_ == NULL) << "Object leak. Pointer must be NULL";
    return &ptr_;
  }

  template <class Query>
  nsresult QueryInterface(Query** p) {
    DCHECK(p != NULL);
    DCHECK(ptr_ != NULL);
    return ptr_->QueryInterface(Query::GetIID(), reinterpret_cast<void**>(p));
  }

  template <class Query>
  nsresult QueryInterface(const nsIID& iid, Query** p) {
    DCHECK(p != NULL);
    DCHECK(ptr_ != NULL);
    return ptr_->QueryInterface(iid, reinterpret_cast<void**>(p));
  }

  // Queries |other| for the interface this object wraps and returns the
  // error code from the other->QueryInterface operation.
  nsresult QueryFrom(nsISupports* other) {
    DCHECK(other != NULL);
    return other->QueryInterface(iid(), reinterpret_cast<void**>(Receive()));
  }

  // Checks if the identity of |other| and this object is the same.
  bool IsSameObject(nsISupports* other) {
    if (!other && !ptr_)
      return true;

    if (!other || !ptr_)
      return false;

    nsIID iid = NS_ISUPPORTS_IID;
    ScopedNsPtr<nsISupports, iid> my_identity;
    QueryInterface(my_identity.Receive());

    ScopedNsPtr<nsISupports, iid> other_identity;
    other->QueryInterface(other_identity.Receive());

    return static_cast<nsISupports*>(my_identity) ==
           static_cast<nsISupports*>(other_identity);
  }

  // Provides direct access to the interface.
  // Here we use a well known trick to make sure we block access to
  // IUknown methods so that something bad like this doesn't happen:
  //    ScopedNsPtr<nsISupports> p(Foo());
  //    p->Release();
  //    ... later the destructor runs, which will Release() again.
  // and to get the benefit of the DCHECKs we add to QueryInterface.
  // There's still a way to call these methods if you absolutely must
  // by statically casting the ScopedNsPtr instance to the wrapped interface
  // and then making the call... but generally that shouldn't be necessary.
  BlocknsISupportsMethods<nsInterface>* operator->() const {
    DCHECK(ptr_ != NULL);
    return reinterpret_cast<BlocknsISupportsMethods<nsInterface>*>(ptr_);
  }

  // static methods

  static const nsIID& iid() {
    return *interface_id;
  }
};

#endif  // CHROME_FRAME_SCOPED_NS_PTR_WIN_H_
