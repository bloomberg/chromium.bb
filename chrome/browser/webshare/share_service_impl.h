// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_

#include <string>

#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/modules/webshare/webshare.mojom.h"

class GURL;

// Desktop implementation of the ShareService Mojo service.
class ShareServiceImpl : public blink::mojom::ShareService {
 public:
  ShareServiceImpl() = default;
  ~ShareServiceImpl() override = default;

  static void Create(mojo::InterfaceRequest<ShareService> request);

  void Share(const std::string& title,
             const std::string& text,
             const GURL& url,
             const ShareCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShareServiceImpl);
};

#endif  // CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
