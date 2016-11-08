// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

class SearchGeolocationDisclosureInfoBarDelegate
    : public infobars::InfoBarDelegate {
 public:
  ~SearchGeolocationDisclosureInfoBarDelegate() override;

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents);

  // Gets the message to display in the infobar.
  base::string16 GetMessageText() const;

 private:
  SearchGeolocationDisclosureInfoBarDelegate();

  // InfoBarDelegate:
  Type GetInfoBarType() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;

  DISALLOW_COPY_AND_ASSIGN(SearchGeolocationDisclosureInfoBarDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE_H_
