// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_CLIENT_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_CLIENT_H_

class HotwordClient {
 public:
  virtual ~HotwordClient() {}

  // Called when the hotword recognition session state has been changed.
  virtual void OnHotwordStateChanged(bool started) {}

  // Called when the hotword is recognized.
  virtual void OnHotwordRecognized() = 0;
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_CLIENT_H_
