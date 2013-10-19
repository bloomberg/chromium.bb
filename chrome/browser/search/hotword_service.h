// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_

#include "base/basictypes.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class Profile;

// Provides an interface for the Hotword component that does voice triggered
// search.
class HotwordService : public BrowserContextKeyedService {
 public:
  explicit HotwordService(Profile* profile);
  virtual ~HotwordService();

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(HotwordService);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_SERVICE_H_
