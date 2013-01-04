// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is used by RenderView to talk to the pepper plugin delegate.
#ifndef CONTENT_RENDERER_RENDER_VIEW_PEPPER_HELPER_H
#define CONTENT_RENDERER_RENDER_VIEW_PEPPER_HELPER_H

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "ui/base/ime/text_input_type.h"

class TransportDIB;

namespace gfx {
class Rect;
}

namespace IPC {
struct ChannelHandle;
}

namespace ui {
class Range;
}

namespace webkit {
struct WebPluginInfo;
namespace ppapi {
class PluginInstance;
}
}

namespace WebKit {
struct WebCompositionUnderline;
struct WebPluginParams;
class WebPlugin;
}

namespace content {

class RenderViewPepperHelper {
 public:
  RenderViewPepperHelper() {}
  virtual ~RenderViewPepperHelper();

  virtual WebKit::WebPlugin* CreatePepperWebPlugin(
      const webkit::WebPluginInfo& webplugin_info,
      const WebKit::WebPluginParams& params);

  // Called by RenderView to implement the corresponding function in its base
  // class RenderWidget (see that for more).
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor);

  // Called by RenderView to tell us about painting events, these two functions
  // just correspond to the WillInitiatePaint, DidInitiatePaint and
  // DidFlushPaint hooks in RenderView.
  virtual void ViewWillInitiatePaint() {}
  virtual void ViewInitiatedPaint() {}
  virtual void ViewFlushedPaint() {}

  // Called by RenderView when ViewMsg_AsyncOpenFile_ACK.
  virtual void OnAsyncFileOpened(base::PlatformFileError error_code,
                                 base::PlatformFile file,
                                 int message_id) {}

  // Called by RenderView when ViewMsg_PpapiBrokerChannelCreated.
  virtual void OnPpapiBrokerChannelCreated(int request_id,
                                           base::ProcessId broker_pid,
                                           const IPC::ChannelHandle& handle) {}

  // Called when we know whether permission to access the PPAPI broker was
  // granted.
  virtual void OnPpapiBrokerPermissionResult(int request_id, bool result) {}

  // Notification that the render view has been focused or defocused. This
  // notifies all of the plugins.
  virtual void OnSetFocus(bool has_focus) {}

  // Notification that the page visibility has changed. The default is visible.
  virtual void PageVisibilityChanged(bool is_visible) {}

  // IME status.
  virtual bool IsPluginFocused() const;
  virtual gfx::Rect GetCaretBounds() const;
  virtual ui::TextInputType GetTextInputType() const;
  virtual bool IsPluginAcceptingCompositionEvents() const;
  virtual bool CanComposeInline() const;
  virtual void GetSurroundingText(string16* text, ui::Range* range) const {}

  // IME events.
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) {}
  virtual void OnImeConfirmComposition(const string16& text) {}

  // Notification that a mouse event has arrived at the render view.
  virtual void WillHandleMouseEvent() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderViewPepperHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_PEPPER_HELPER_H
