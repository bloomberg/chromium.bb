// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_IE8_TYPES_H_
#define CHROME_FRAME_IE8_TYPES_H_

#include <urlmon.h>

#ifndef __IInternetBindInfoEx_INTERFACE_DEFINED__
#define __IInternetBindInfoEx_INTERFACE_DEFINED__

#define IID_IInternetBindInfoEx (__uuidof(IInternetBindInfoEx))

MIDL_INTERFACE("A3E015B7-A82C-4DCD-A150-569AEEED36AB")
IInternetBindInfoEx : public IInternetBindInfo {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetBindInfoEx(
      DWORD *grfBINDF,
      BINDINFO *pbindinfo,
      DWORD *grfBINDF2,
      DWORD *pdwReserved) = 0;
};

#endif __IInternetBindInfoEx_INTERFACE_DEFINED__

#endif //  CHROME_FRAME_IE8_TYPES_H_
