// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_scrollbar_api.h"
#include "ppapi/thunk/ppb_widget_api.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebPluginScrollbarClient.h"
#include "ui/gfx/rect.h"

namespace content {

class PPB_Scrollbar_Impl : public ppapi::Resource,
                           public ppapi::thunk::PPB_Scrollbar_API,
                           public ppapi::thunk::PPB_Widget_API,
                           public WebKit::WebPluginScrollbarClient {
 public:
  static PP_Resource Create(PP_Instance instance, bool vertical);

  // Resource overrides.
  virtual PPB_Scrollbar_API* AsPPB_Scrollbar_API() OVERRIDE;
  virtual PPB_Widget_API* AsPPB_Widget_API() OVERRIDE;
  virtual void InstanceWasDeleted() OVERRIDE;

  // PPB_Scrollbar_API implementation.
  virtual uint32_t GetThickness() OVERRIDE;
  virtual bool IsOverlay() OVERRIDE;
  virtual uint32_t GetValue() OVERRIDE;
  virtual void SetValue(uint32_t value) OVERRIDE;
  virtual void SetDocumentSize(uint32_t size) OVERRIDE;
  virtual void SetTickMarks(const PP_Rect* tick_marks, uint32_t count) OVERRIDE;
  virtual void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) OVERRIDE;

  // PPB_Widget_API implementation.
  virtual PP_Bool Paint(const PP_Rect* pp_rect, PP_Resource image_id) OVERRIDE;
  virtual PP_Bool HandleEvent(PP_Resource pp_input_event) OVERRIDE;
  virtual PP_Bool GetLocation(PP_Rect* location) OVERRIDE;
  virtual void SetLocation(const PP_Rect* location) OVERRIDE;
  virtual void SetScale(float scale) OVERRIDE;

 private:
  virtual ~PPB_Scrollbar_Impl();

  explicit PPB_Scrollbar_Impl(PP_Instance instance);
  void Init(bool vertical);

  // WebKit::WebPluginScrollbarClient implementation.
  virtual void valueChanged(WebKit::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void overlayChanged(WebKit::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void invalidateScrollbarRect(WebKit::WebPluginScrollbar* scrollbar,
                                       const WebKit::WebRect& rect) OVERRIDE;
  virtual void getTickmarks(
      WebKit::WebPluginScrollbar* scrollbar,
      WebKit::WebVector<WebKit::WebRect>* tick_marks) const OVERRIDE;

  void Invalidate(const PP_Rect& dirty);
  void NotifyInvalidate();

  PP_Rect location_;
  float scale_;
  gfx::Rect dirty_;
  std::vector<WebKit::WebRect> tickmarks_;
  scoped_ptr<WebKit::WebPluginScrollbar> scrollbar_;

  // Used so that the post task for Invalidate doesn't keep an extra reference.
  base::WeakPtrFactory<PPB_Scrollbar_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Scrollbar_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
