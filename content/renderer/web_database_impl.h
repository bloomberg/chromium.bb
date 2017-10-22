// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_DATABASE_IMPL_H_
#define CONTENT_RENDERER_WEB_DATABASE_IMPL_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "content/common/web_database.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

// Receives database messages from the browser process and processes them on the
// IO thread.
class WebDatabaseImpl : public content::mojom::WebDatabase {
 public:
  WebDatabaseImpl();
  ~WebDatabaseImpl() override;

  static void Create(content::mojom::WebDatabaseRequest);

 private:
  // content::mojom::Database:
  void UpdateSize(const url::Origin& origin,
                  const base::string16& name,
                  int64_t size) override;
  void CloseImmediately(const url::Origin& origin,
                        const base::string16& name) override;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_DATABASE_IMPL_H_
