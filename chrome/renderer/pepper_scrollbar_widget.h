// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_
#define CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_
#pragma once

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/renderer/pepper_widget.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScrollbarClient.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

// An implementation of a horizontal/vertical scrollbar.
class PepperScrollbarWidget : public PepperWidget,
                              public WebKit::WebScrollbarClient,
                              public base::RefCounted<PepperScrollbarWidget> {
 public:
  explicit PepperScrollbarWidget(const NPScrollbarCreateParams& params);

  // PepperWidget
  virtual void Destroy();
  virtual void Paint(Graphics2DDeviceContext* context, const NPRect& dirty);
  virtual bool HandleEvent(const NPPepperEvent& event);
  virtual void GetProperty(NPWidgetProperty property, void* value);
  virtual void SetProperty(NPWidgetProperty property, void* value);

  // WebKit::WebScrollbarClient
  virtual void valueChanged(WebKit::WebScrollbar*);
  virtual void invalidateScrollbarRect(WebKit::WebScrollbar*,
                                       const WebKit::WebRect&);
  virtual void getTickmarks(WebKit::WebScrollbar*,
                            WebKit::WebVector<WebKit::WebRect>*) const;

#if defined(OS_LINUX)
  static void SetScrollbarColors(unsigned inactive_color,
                                 unsigned active_color,
                                 unsigned track_color);
#endif

 private:
  friend class base::RefCounted<PepperScrollbarWidget>;

  ~PepperScrollbarWidget();

  void NotifyInvalidate();

  gfx::Rect dirty_rect_;
  gfx::Rect location_;
  std::vector<WebKit::WebRect> tickmarks_;
  scoped_ptr<WebKit::WebScrollbar> scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(PepperScrollbarWidget);
};

#endif  // CHROME_RENDERER_PEPPER_SCROLLBAR_WIDGET_H_
