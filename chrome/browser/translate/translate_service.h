// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_

// Singleton managing the resources required for Translate.
class TranslateService {
 public:

   // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown(bool cleanup_pending_fetcher);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
