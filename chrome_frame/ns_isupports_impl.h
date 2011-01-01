// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_NS_ISUPPORTS_IMPL_H_
#define CHROME_FRAME_NS_ISUPPORTS_IMPL_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "chrome_frame/utils.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nscore.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsid.h"
#include "third_party/xulrunner-sdk/win/include/xpcom/nsISupportsBase.h"

// A simple single-threaded implementation of methods needed to support
// nsISupports. Must be inherited by classes that also inherit from nsISupports.
template<class Derived>
class NsISupportsImplBase {
 public:
  NsISupportsImplBase() : nsiimpl_ref_count_(0) {
    nsiimpl_thread_id_ = base::PlatformThread::CurrentId();
  }

  virtual ~NsISupportsImplBase() {
    DCHECK(nsiimpl_thread_id_ == base::PlatformThread::CurrentId());
  }

  NS_IMETHOD QueryInterface(REFNSIID iid, void** ptr) {
    DCHECK(nsiimpl_thread_id_ == base::PlatformThread::CurrentId());
    nsresult res = NS_NOINTERFACE;

    if (memcmp(&iid, &__uuidof(nsISupports), sizeof(nsIID)) == 0) {
      *ptr = static_cast<nsISupports*>(static_cast<typename Derived*>(this));
      AddRef();
      res = NS_OK;
    }

    return res;
  }

  NS_IMETHOD_(nsrefcnt) AddRef() {
    DCHECK(nsiimpl_thread_id_ == base::PlatformThread::CurrentId());
    nsiimpl_ref_count_++;
    return nsiimpl_ref_count_;
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    DCHECK(nsiimpl_thread_id_ == base::PlatformThread::CurrentId());
    nsiimpl_ref_count_--;

    if (!nsiimpl_ref_count_) {
      Derived* me = static_cast<Derived*>(this);
      delete me;
      return 0;
    }

    return nsiimpl_ref_count_;
  }

 protected:
  nsrefcnt nsiimpl_ref_count_;
  AddRefModule nsiimpl_module_ref_;
  // used to DCHECK on expected single-threaded usage
  uint64 nsiimpl_thread_id_;
};

#endif  // CHROME_FRAME_NS_ISUPPORTS_IMPL_H_
