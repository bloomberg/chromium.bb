// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_SCOPED_AUTHORIZATIONREF_H_
#define CHROME_BROWSER_COCOA_SCOPED_AUTHORIZATIONREF_H_
#pragma once

#include <Security/Authorization.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

// scoped_AuthorizationRef maintains ownership of an AuthorizationRef.  It is
// patterned after the scoped_ptr interface.

class scoped_AuthorizationRef {
 public:
  explicit scoped_AuthorizationRef(AuthorizationRef authorization = NULL)
      : authorization_(authorization) {
  }

  ~scoped_AuthorizationRef() {
    if (authorization_) {
      AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
    }
  }

  void reset(AuthorizationRef authorization = NULL) {
    if (authorization_ != authorization) {
      if (authorization_) {
        AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
      }
      authorization_ = authorization;
    }
  }

  bool operator==(AuthorizationRef that) const {
    return authorization_ == that;
  }

  bool operator!=(AuthorizationRef that) const {
    return authorization_ != that;
  }

  operator AuthorizationRef() const {
    return authorization_;
  }

  AuthorizationRef* operator&() {
    return &authorization_;
  }

  AuthorizationRef get() const {
    return authorization_;
  }

  void swap(scoped_AuthorizationRef& that) {
    AuthorizationRef temp = that.authorization_;
    that.authorization_ = authorization_;
    authorization_ = temp;
  }

  // scoped_AuthorizationRef::release() is like scoped_ptr<>::release.  It is
  // NOT a wrapper for AuthorizationFree().  To force a
  // scoped_AuthorizationRef object to call AuthorizationFree(), use
  // scoped_AuthorizaitonRef::reset().
  AuthorizationRef release() WARN_UNUSED_RESULT {
    AuthorizationRef temp = authorization_;
    authorization_ = NULL;
    return temp;
  }

 private:
  AuthorizationRef authorization_;

  DISALLOW_COPY_AND_ASSIGN(scoped_AuthorizationRef);
};

#endif  // CHROME_BROWSER_COCOA_SCOPED_AUTHORIZATIONREF_H_
