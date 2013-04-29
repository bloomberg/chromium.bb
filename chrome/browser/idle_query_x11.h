// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IDLE_QUERY_X11_H_
#define CHROME_BROWSER_IDLE_QUERY_X11_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace chrome {

class IdleData;

class IdleQueryX11 {
 public:
  IdleQueryX11();
  ~IdleQueryX11();

  int IdleTime();

 private:
  scoped_ptr<IdleData> idle_data_;

  DISALLOW_COPY_AND_ASSIGN(IdleQueryX11);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_IDLE_QUERY_X11_H_
