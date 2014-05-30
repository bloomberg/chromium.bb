// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include "base/macros.h"
#include "components/history/core/browser/history_client.h"

class ChromeHistoryClient : public history::HistoryClient {
 public:
  ChromeHistoryClient();

 protected:
  DISALLOW_COPY_AND_ASSIGN(ChromeHistoryClient);
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
