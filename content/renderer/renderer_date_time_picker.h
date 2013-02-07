// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_DATE_TIME_PICKER_H_
#define CONTENT_RENDERER_RENDERER_DATE_TIME_PICKER_H_

#include "base/basictypes.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDateTimeChooserParams.h"

struct ViewHostMsg_DateTimeDialogValue_Params;

namespace WebKit {
class WebDateTimeChooserCompletion;
}  // namespace WebKit

namespace content {
class RenderViewImpl;

class RendererDateTimePicker : public RenderViewObserver {
 public:
  RendererDateTimePicker(
      RenderViewImpl* sender,
      const WebKit::WebDateTimeChooserParams& params,
      WebKit::WebDateTimeChooserCompletion* completion);
  virtual ~RendererDateTimePicker();

  bool Open();

 private:
  void OnReplaceDateTime(const ViewHostMsg_DateTimeDialogValue_Params& value);
  void OnCancel();

  // RenderViewObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  WebKit::WebDateTimeChooserParams chooser_params_;
  WebKit::WebDateTimeChooserCompletion* chooser_completion_;  // Not owned by us

  DISALLOW_COPY_AND_ASSIGN(RendererDateTimePicker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_DATE_TIME_PICKER_H_
