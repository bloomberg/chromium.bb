// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_
#define COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_

#include "components/signin/core/webdata/token_web_data.h"

class TokenWebData;

// An interface that needs to be supplied to the Signin component by its
// embedder.
class SigninClient {
 public:
  virtual ~SigninClient() {}

  // Gets the TokenWebData instance associated with the client.
  virtual scoped_refptr<TokenWebData> GetDatabase() = 0;
};

#endif  // COMPONENTS_SIGNIN_CORE_SIGNIN_CLIENT_H_
