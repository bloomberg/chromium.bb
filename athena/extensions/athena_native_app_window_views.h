// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_ATHENA_NATIVE_APP_WINDOW_VIEWS_H_
#define ATHENA_EXTENSIONS_ATHENA_NATIVE_APP_WINDOW_VIEWS_H_

#include "components/native_app_window/native_app_window_views.h"

namespace athena {

class AthenaNativeAppWindowViews
    : public native_app_window::NativeAppWindowViews {
 public:
  AthenaNativeAppWindowViews() {}
  virtual ~AthenaNativeAppWindowViews() {}

  views::WebView* GetWebView();

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaNativeAppWindowViews);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_ATHENA_NATIVE_APP_WINDOW_VIEWS_H_
