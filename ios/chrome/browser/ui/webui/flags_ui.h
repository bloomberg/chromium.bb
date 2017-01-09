// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/webui/web_ui_ios_controller.h"

class BaseFlagsUI : public web::WebUIIOSController {
 public:
  enum FlagsUIKind {
    FLAGS_UI_GENERIC,
    FLAGS_UI_APPLE,
  };

  BaseFlagsUI(web::WebUIIOS* web_ui, FlagsUIKind flags_ui_kind);
  ~BaseFlagsUI() override;

 private:
  void Initialize(web::WebUIIOS* web_ui, FlagsUIKind flags_ui_kind);

  base::WeakPtrFactory<BaseFlagsUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BaseFlagsUI);
};

class FlagsUI : public BaseFlagsUI {
 public:
  explicit FlagsUI(web::WebUIIOS* web_ui);
  ~FlagsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FlagsUI);
};

class AppleFlagsUI : public BaseFlagsUI {
 public:
  explicit AppleFlagsUI(web::WebUIIOS* web_ui);
  ~AppleFlagsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppleFlagsUI);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
