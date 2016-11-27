// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_H_
#define CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/favicon/core/fallback_icon_client.h"

// ChromeFallbackIconClient implements the FallbackIconClient interface.
class ChromeFallbackIconClient : public favicon::FallbackIconClient {
 public:
  ChromeFallbackIconClient();
  ~ChromeFallbackIconClient() override;

  // FallbackIconClient implementation:
  const std::vector<std::string>& GetFontNameList() const override;

 private:
  std::vector<std::string> font_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFallbackIconClient);
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FALLBACK_ICON_CLIENT_H_
