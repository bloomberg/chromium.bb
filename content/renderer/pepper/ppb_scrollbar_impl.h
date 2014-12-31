// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/pepper/ppb_widget_impl.h"
#include "ppapi/thunk/ppb_scrollbar_api.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebPluginScrollbarClient.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class PPB_Scrollbar_Impl : public PPB_Widget_Impl,
                           public ppapi::thunk::PPB_Scrollbar_API,
                           public blink::WebPluginScrollbarClient {
 public:
  static PP_Resource Create(PP_Instance instance, bool vertical);

  // Resource overrides.
  PPB_Scrollbar_API* AsPPB_Scrollbar_API() override;
  void InstanceWasDeleted() override;

  // PPB_Scrollbar_API implementation.
  uint32_t GetThickness() override;
  bool IsOverlay() override;
  uint32_t GetValue() override;
  void SetValue(uint32_t value) override;
  void SetDocumentSize(uint32_t size) override;
  void SetTickMarks(const PP_Rect* tick_marks, uint32_t count) override;
  void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) override;

 private:
  ~PPB_Scrollbar_Impl() override;

  explicit PPB_Scrollbar_Impl(PP_Instance instance);
  void Init(bool vertical);

  // PPB_Widget private implementation.
  PP_Bool PaintInternal(const gfx::Rect& rect,
                        PPB_ImageData_Impl* image) override;
  PP_Bool HandleEventInternal(const ppapi::InputEventData& data) override;
  void SetLocationInternal(const PP_Rect* location) override;

  // blink::WebPluginScrollbarClient implementation.
  virtual void valueChanged(blink::WebPluginScrollbar* scrollbar) override;
  virtual void overlayChanged(blink::WebPluginScrollbar* scrollbar) override;
  virtual void invalidateScrollbarRect(blink::WebPluginScrollbar* scrollbar,
                                       const blink::WebRect& rect) override;
  virtual void getTickmarks(blink::WebPluginScrollbar* scrollbar,
                            blink::WebVector<blink::WebRect>* tick_marks) const
      override;

  void NotifyInvalidate();

  gfx::Rect dirty_;
  std::vector<blink::WebRect> tickmarks_;
  scoped_ptr<blink::WebPluginScrollbar> scrollbar_;

  // Used so that the post task for Invalidate doesn't keep an extra reference.
  base::WeakPtrFactory<PPB_Scrollbar_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Scrollbar_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
