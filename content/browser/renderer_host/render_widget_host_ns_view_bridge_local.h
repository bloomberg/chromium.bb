// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_LOCAL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_LOCAL_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "content/browser/renderer_host/popup_window_mac.h"
#import "content/browser/renderer_host/render_widget_host_ns_view_bridge.h"
#import "content/browser/renderer_host/render_widget_host_view_cocoa.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/display/display_observer.h"

namespace content {

// Bridge to a locally-hosted NSView -- this is always instantiated in the same
// process as the NSView. The caller of this interface may exist in another
// process.
class RenderWidgetHostNSViewBridgeLocal : public RenderWidgetHostNSViewBridge,
                                          public display::DisplayObserver {
 public:
  explicit RenderWidgetHostNSViewBridgeLocal(
      RenderWidgetHostNSViewClient* client);
  ~RenderWidgetHostNSViewBridgeLocal() override;

  // RenderWidgetHostNSViewBridge implementation.
  RenderWidgetHostViewCocoa* GetRenderWidgetHostViewCocoa() override;
  void InitAsPopup(const gfx::Rect& content_rect,
                   blink::WebPopupType popup_type) override;
  void DisableDisplay() override;
  void MakeFirstResponder() override;
  void SetBounds(const gfx::Rect& rect) override;
  void SetCALayerParams(const gfx::CALayerParams& ca_layer_params) override;
  void SetBackgroundColor(SkColor color) override;
  void SetVisible(bool visible) override;
  void SetTooltipText(const base::string16& display_text) override;
  void SetTextInputType(ui::TextInputType text_input_type) override;
  void SetTextSelection(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  void SetCompositionRangeInfo(const gfx::Range& range) override;
  void CancelComposition() override;
  void SetShowingContextMenu(bool showing) override;
  void DisplayCursor(const WebCursor& cursor) override;
  void SetCursorLocked(bool locked) override;
  void ShowDictionaryOverlayForSelection() override;
  void ShowDictionaryOverlay(
      const mac::AttributedStringCoder::EncodedString& encoded_string,
      gfx::Point baseline_point) override;
  void LockKeyboard(base::Optional<base::flat_set<ui::DomCode>> codes) override;
  void UnlockKeyboard() override;

 private:
  bool IsPopup() const {
    // TODO(ccameron): If this is not equivalent to |popup_window_| then
    // there are bugs.
    return popup_type_ != blink::kWebPopupTypeNone;
  }

  // display::DisplayObserver implementation.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // The NSView used for input and display.
  base::scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view_;

  // Once set, all calls to set the background color or CALayer content will
  // be ignored.
  bool display_disabled_ = false;

  // The window used for popup widgets, and its helper.
  std::unique_ptr<PopupWindowMac> popup_window_;
  blink::WebPopupType popup_type_ = blink::kWebPopupTypeNone;

  // The background CALayer which is hosted by |cocoa_view_|, and is used as
  // the root of |display_ca_layer_tree_|.
  base::scoped_nsobject<CALayer> background_layer_;
  std::unique_ptr<ui::DisplayCALayerTree> display_ca_layer_tree_;

  // Cached copy of the tooltip text, to avoid redundant calls.
  base::string16 tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewBridgeLocal);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_LOCAL_H_
