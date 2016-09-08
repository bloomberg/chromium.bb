// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_DELEGATE_H_

namespace arc {

class ArcAuthCodeFetcherDelegate {
 public:
  virtual void OnAuthCodeSuccess(const std::string& auth_code) = 0;
  virtual void OnAuthCodeFailed() = 0;

 protected:
  virtual ~ArcAuthCodeFetcherDelegate() {}
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_DELEGATE_H_
