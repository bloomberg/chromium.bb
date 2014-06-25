// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class GuestViewInternalCreateGuestFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("guestViewInternal.createGuest",
                             GUESTVIEWINTERNAL_CREATEGUEST);
  GuestViewInternalCreateGuestFunction();

 protected:
  virtual ~GuestViewInternalCreateGuestFunction() {}
  virtual bool RunAsync() OVERRIDE FINAL;

 private:
  void CreateGuestCallback(content::WebContents* guest_web_contents);
  DISALLOW_COPY_AND_ASSIGN(GuestViewInternalCreateGuestFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GUEST_VIEW_GUEST_VIEW_INTERNAL_API_H_
