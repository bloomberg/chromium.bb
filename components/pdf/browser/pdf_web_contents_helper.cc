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
#include "ui/gfx/geometry/point_conversions.h"
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
      has_selection_(false),
      remote_pdf_client_(nullptr),
      web_contents_(web_contents) {}

PDFWebContentsHelper::~PDFWebContentsHelper() {
  if (!touch_selection_controller_client_manager_)
    return;

  touch_selection_controller_client_manager_->InvalidateClient(this);
  touch_selection_controller_client_manager_->RemoveObserver(this);
}

void PDFWebContentsHelper::SetListener(mojom::PdfListenerPtr listener) {
  remote_pdf_client_ = std::move(listener);
}

gfx::PointF PDFWebContentsHelper::ConvertHelper(const gfx::PointF& point_f,
                                                float scale) const {
  gfx::PointF origin_f;
  content::RenderWidgetHostView* view =
      web_contents_->GetRenderWidgetHostView();
  if (view) {
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());
    origin_f.Scale(scale);
  }

  return gfx::PointF(point_f.x() + origin_f.x(), point_f.y() + origin_f.y());
}

gfx::PointF PDFWebContentsHelper::ConvertFromRoot(
    const gfx::PointF& point_f) const {
  return ConvertHelper(point_f, -1.f);
}

gfx::PointF PDFWebContentsHelper::ConvertToRoot(
    const gfx::PointF& point_f) const {
  return ConvertHelper(point_f, +1.f);
}

void PDFWebContentsHelper::SelectionChanged(const gfx::PointF& left,
                                            int32_t left_height,
                                            const gfx::PointF& right,
                                            int32_t right_height) {
  if (!touch_selection_controller_client_manager_)
    InitTouchSelectionClientManager();

  if (touch_selection_controller_client_manager_) {
    gfx::SelectionBound start;
    gfx::SelectionBound end;
    start.SetEdgeTop(ConvertToRoot(left));
    start.SetEdgeBottom(
        ConvertToRoot(gfx::PointF(left.x(), left.y() + left_height)));
    end.SetEdgeTop(ConvertToRoot(right));
    end.SetEdgeBottom(
        ConvertToRoot(gfx::PointF(right.x(), right.y() + right_height)));

    // Don't do left/right comparison after setting type.
    // TODO(wjmaclean): When PDFium supports editing, we'll need to detect
    // start == end as *either* no selection, or an insertion point.
    has_selection_ = start != end;
    start.set_visible(has_selection_);
    end.set_visible(has_selection_);
    start.set_type(has_selection_ ? gfx::SelectionBound::LEFT
                                  : gfx::SelectionBound::EMPTY);
    end.set_type(has_selection_ ? gfx::SelectionBound::RIGHT
                                : gfx::SelectionBound::EMPTY);

    touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
        start, end, this, this);
  }
}

bool PDFWebContentsHelper::SupportsAnimation() const {
  return false;
}

void PDFWebContentsHelper::MoveCaret(const gfx::PointF& position) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->SetCaretPosition(ConvertFromRoot(position));
}

void PDFWebContentsHelper::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void PDFWebContentsHelper::SelectBetweenCoordinates(const gfx::PointF& base,
                                                    const gfx::PointF& extent) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->SetSelectionBounds(ConvertFromRoot(base),
                                         ConvertFromRoot(extent));
}

void PDFWebContentsHelper::OnSelectionEvent(ui::SelectionEventType event) {}

std::unique_ptr<ui::TouchHandleDrawable>
PDFWebContentsHelper::CreateDrawable() {
  // We can return null here, as the manager will look after this.
  return std::unique_ptr<ui::TouchHandleDrawable>();
}

void PDFWebContentsHelper::DidScroll() {
  // Nothing to do here, as PDFiumEngine sends a new selection for each scroll
  // update.
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
      // TODO(wjmaclean): add logic for cut/paste as the information required
      // from PDFium becomes available.
  }
  return false;
}

void PDFWebContentsHelper::ExecuteCommand(int command_id, int event_flags) {
  // TODO(wjmaclean, dsinclair): Need to communicate to PDFium to accept
  // cut/paste commands.
  switch (command_id) {
    case IDS_APP_COPY:
      web_contents()->Copy();
      break;
  }
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
