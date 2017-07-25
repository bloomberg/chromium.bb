// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/strings/grit/ui_strings.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(pdf::PDFWebContentsHelper);

namespace pdf {

// static
void PDFWebContentsHelper::CreateForWebContentsWithClient(
    content::WebContents* contents,
    std::unique_ptr<PDFWebContentsHelperClient> client) {
  if (FromWebContents(contents))
    return;
  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PDFWebContentsHelper(contents, std::move(client))));
}

PDFWebContentsHelper::PDFWebContentsHelper(
    content::WebContents* web_contents,
    std::unique_ptr<PDFWebContentsHelperClient> client)
    : content::WebContentsObserver(web_contents),
      pdf_service_bindings_(web_contents, this),
      client_(std::move(client)),
      touch_selection_controller_client_manager_(nullptr),
      has_selection_(false) {}

PDFWebContentsHelper::~PDFWebContentsHelper() {
  if (!touch_selection_controller_client_manager_)
    return;

  touch_selection_controller_client_manager_->InvalidateClient(this);
  touch_selection_controller_client_manager_->RemoveObserver(this);
}

void PDFWebContentsHelper::SelectionChanged(const gfx::Point& left,
                                            int32_t left_height,
                                            const gfx::Point& right,
                                            int32_t right_height) {
  if (!touch_selection_controller_client_manager_)
    InitTouchSelectionClientManager();

  if (touch_selection_controller_client_manager_) {
    gfx::SelectionBound start;
    gfx::SelectionBound end;
    start.SetEdgeTop(gfx::PointF(left.x(), left.y()));
    start.SetEdgeBottom(gfx::PointF(left.x(), left.y() + left_height));
    start.set_type(gfx::SelectionBound::LEFT);
    start.set_visible(true);
    end.SetEdgeTop(gfx::PointF(right.x(), right.y()));
    end.SetEdgeBottom(gfx::PointF(right.x(), right.y() + right_height));
    end.set_type(gfx::SelectionBound::RIGHT);
    end.set_visible(true);

    has_selection_ = start != end;

    touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
        start, end, this, this);
  }
}

bool PDFWebContentsHelper::SupportsAnimation() const {
  return false;
}

void PDFWebContentsHelper::MoveCaret(const gfx::PointF& position) {
  // TODO(wjmaclean, dsinclair): Implement connection to PDFium to implement.
}

void PDFWebContentsHelper::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  // TODO(wjmaclean, dsinclair): Implement connection to PDFium to implement.
}

void PDFWebContentsHelper::SelectBetweenCoordinates(const gfx::PointF& base,
                                                    const gfx::PointF& extent) {
  // TODO(wjmaclean, dsinclair): Implement connection to PDFium to implement.
}

void PDFWebContentsHelper::OnSelectionEvent(ui::SelectionEventType event) {}

std::unique_ptr<ui::TouchHandleDrawable>
PDFWebContentsHelper::CreateDrawable() {
  // We can return null here, as the manager will look after this.
  return std::unique_ptr<ui::TouchHandleDrawable>();
}

void PDFWebContentsHelper::OnManagerWillDestroy(
    content::TouchSelectionControllerClientManager* manager) {
  DCHECK(manager == touch_selection_controller_client_manager_);
  manager->RemoveObserver(this);
  touch_selection_controller_client_manager_ = nullptr;
}

bool PDFWebContentsHelper::IsCommandIdEnabled(int command_id) const {
  // TODO(wjmaclean|dsinclair): Make PDFium send readability information in the
  // selection changed message?
  bool readable = true;

  switch (command_id) {
    case IDS_APP_COPY:
      return readable && has_selection_;
      // TODO(wjmaclean): add logic for copy/paste as the information required
      // from PDFium becomes available.
  }
  return false;
}

void PDFWebContentsHelper::ExecuteCommand(int command_id, int event_flags) {
  // TODO(wjmaclean, dsinclair): Need to communicate to PDFium to get it to copy
  // the selection onto the clipboard (and eventually accept cut/paste commands
  // too).
}

void PDFWebContentsHelper::RunContextMenu() {
  // TouchSelectionControllerClientAura will handle this for us.
  NOTIMPLEMENTED();
}

void PDFWebContentsHelper::InitTouchSelectionClientManager() {
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (!view)
    return;

  touch_selection_controller_client_manager_ =
      view->GetTouchSelectionControllerClientManager();
  if (!touch_selection_controller_client_manager_)
    return;

  touch_selection_controller_client_manager_->AddObserver(this);
}

void PDFWebContentsHelper::HasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(web_contents());
}

void PDFWebContentsHelper::SaveUrlAs(const GURL& url,
                                     const content::Referrer& referrer) {
  client_->OnSaveURL(web_contents());
  web_contents()->SaveFrame(url, referrer);
}

void PDFWebContentsHelper::UpdateContentRestrictions(
    int32_t content_restrictions) {
  client_->UpdateContentRestrictions(web_contents(), content_restrictions);
}

}  // namespace pdf
