// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/ax_tree_source_aura.h"

#include "chrome/common/extensions/api/automation_api_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"
#include "ui/views/controls/webview/webview.h"

AXTreeSourceAura::AXTreeSourceAura()
    : desktop_root_(std::make_unique<AXRootObjWrapper>()) {}

AXTreeSourceAura::~AXTreeSourceAura() = default;

bool AXTreeSourceAura::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = extensions::api::automation::kDesktopTreeID;
  return AXTreeSourceViews::GetTreeData(tree_data);
}

views::AXAuraObjWrapper* AXTreeSourceAura::GetRoot() const {
  return desktop_root_.get();
}

void AXTreeSourceAura::SerializeNode(views::AXAuraObjWrapper* node,
                                     ui::AXNodeData* out_data) const {
  AXTreeSourceViews::SerializeNode(node, out_data);

  if (out_data->role == ax::mojom::Role::kWebView) {
    views::View* view = static_cast<views::AXViewObjWrapper*>(node)->view();
    content::WebContents* contents =
        static_cast<views::WebView*>(view)->GetWebContents();
    content::RenderFrameHost* rfh = contents->GetMainFrame();
    if (rfh) {
      int ax_tree_id = rfh->GetAXTreeID();
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kChildTreeId,
                                ax_tree_id);
    }
  } else if (out_data->role == ax::mojom::Role::kWindow ||
             out_data->role == ax::mojom::Role::kDialog) {
    // Add clips children flag by default to these roles.
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  }
}
