// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_DELEGATE_H_

namespace arc {

class ArcAuthContextDelegate {
 public:
  virtual void OnContextReady() = 0;
  virtual void OnPrepareContextFailed() = 0;

 protected:
  virtual ~ArcAuthContextDelegate() {}
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_DELEGATE_H_
