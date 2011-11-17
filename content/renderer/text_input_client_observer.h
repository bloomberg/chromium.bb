// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
#define CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/public/renderer/render_view_observer.h"
#include "ui/base/range/range.h"
#include "ui/gfx/point.h"

class RenderViewImpl;

namespace WebKit {
class WebView;
}

// This is the renderer-side message filter that generates the replies for the
// messages sent by the TextInputClientMac. See
// content/browser/renderer_host/text_input_client_mac.h for more information.
class TextInputClientObserver : public content::RenderViewObserver {
 public:
  explicit TextInputClientObserver(RenderViewImpl* render_view);
  virtual ~TextInputClientObserver();

  // RenderViewObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Returns the WebView of the RenderView.
  WebKit::WebView* webview();

  // IPC Message handlers:
  void OnCharacterIndexForPoint(gfx::Point point);
  void OnFirstRectForCharacterRange(ui::Range range);
  void OnStringForRange(ui::Range range);

  DISALLOW_COPY_AND_ASSIGN(TextInputClientObserver);
};

#endif  // CONTENT_RENDERER_TEXT_INPUT_CLIENT_OBSERVER_H_
