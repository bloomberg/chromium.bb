// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_CONNECTION_H_
#define BLIMP_NET_BLIMP_CONNECTION_H_

#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class BLIMP_NET_EXPORT BlimpConnection {
 public:
  BlimpConnection();
  ~BlimpConnection();

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpConnection);
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_H_
