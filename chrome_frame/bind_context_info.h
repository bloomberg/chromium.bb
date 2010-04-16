// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BIND_CONTEXT_INFO_
#define CHROME_FRAME_BIND_CONTEXT_INFO_

#include <atlbase.h>
#include <atlcom.h>

#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"

// This class maintains contextual information used by ChromeFrame.
// This information is maintained in the bind context.
class BindContextInfo : public IUnknown, public CComObjectRoot {
 public:
  BindContextInfo();
  virtual ~BindContextInfo() {}

  BEGIN_COM_MAP(BindContextInfo)
  END_COM_MAP()

  // Returns the BindContextInfo instance associated with the bind
  // context. Creates it if needed.
  static BindContextInfo* FromBindContext(IBindCtx* bind_context);

  void set_chrome_request(bool chrome_request) {
    chrome_request_ = chrome_request;
  }

  bool chrome_request() const {
    return chrome_request_;
  }

  void set_no_cache(bool no_cache) {
    no_cache_ = no_cache;
  }

  bool no_cache() const {
    return no_cache_;
  }

  bool is_switching() const {
    return is_switching_;
  }

  void SetToSwitch(IStream* cache);

  IStream* cache() {
    return cache_;
  }

 private:
  ScopedComPtr<IStream> cache_;
  bool no_cache_;
  bool chrome_request_;
  bool is_switching_;

  DISALLOW_COPY_AND_ASSIGN(BindContextInfo);
};

#endif  // CHROME_FRAME_BIND_CONTEXT_INFO_

