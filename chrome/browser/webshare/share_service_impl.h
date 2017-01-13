// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/modules/webshare/webshare.mojom.h"

class GURL;

// Desktop implementation of the ShareService Mojo service.
class ShareServiceImpl : public blink::mojom::ShareService {
 public:
  ShareServiceImpl() = default;
  ~ShareServiceImpl() override = default;

  static void Create(mojo::InterfaceRequest<ShareService> request);

  // blink::mojom::ShareService overrides:
  void Share(const std::string& title,
             const std::string& text,
             const GURL& share_url,
             const ShareCallback& callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShareServiceImplUnittest, ReplacePlaceholders);

  // Opens a new tab and navigates to |target_url|.
  // Virtual for testing purposes.
  virtual void OpenTargetURL(const GURL& target_url);

  // Writes to |url_template_filled|, a copy of |url_template| with all
  // instances of "{title}", "{text}", and "{url}" replaced with
  // |title|, |text|, and |url| respectively.
  // Replaces instances of "{X}" where "X" is any string besides "title",
  // "text", and "url", with an empty string, for forwards compatibility.
  // Returns false, if there are badly nested placeholders.
  // This includes any case in which two "{" occur before a "}", or a "}"
  // occurs with no preceding "{".
  static bool ReplacePlaceholders(base::StringPiece url_template,
                                  base::StringPiece title,
                                  base::StringPiece text,
                                  const GURL& share_url,
                                  std::string* url_template_filled);

  DISALLOW_COPY_AND_ASSIGN(ShareServiceImpl);
};

#endif  // CHROME_BROWSER_WEBSHARE_SHARE_SERVICE_IMPL_H_
