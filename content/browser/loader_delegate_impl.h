// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_

#include "content/browser/loader/loader_delegate.h"

namespace content {

class LoaderDelegateImpl : public LoaderDelegate {
 public:
  ~LoaderDelegateImpl() override;

  void LoadStateChanged(int child_id,
                        int route_id,
                        const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64_t upload_position,
                        uint64_t upload_size) override;

 private:
  void NotifyLoadStateChangedOnUI(int child_id,
                                  int route_id,
                                  const GURL& url,
                                  const net::LoadStateWithParam& load_state,
                                  uint64_t upload_position,
                                  uint64_t upload_size);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DELEGATE_IMPL_H_
