// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_web_view.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/answer_card_contents_registry.h"
#include "ash/public/interfaces/web_contents_manager.mojom.h"
#include "base/callback.h"
#include "base/unguessable_token.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// AssistantWebView ------------------------------------------------------------

AssistantWebView::AssistantWebView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      web_contents_request_factory_(this) {
  InitLayout();

  assistant_controller_->AddObserver(this);
}

AssistantWebView::~AssistantWebView() {
  assistant_controller_->RemoveObserver(this);
  ReleaseWebContents();
}

gfx::Size AssistantWebView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantWebView::GetHeightForWidth(int width) const {
  return kMaxHeightDip;
}

void AssistantWebView::ChildPreferredSizeChanged(views::View* child) {
  // Because AssistantWebView has a fixed size, it does not re-layout its
  // children when their preferred size changes. To address this, we need to
  // explicitly request a layout pass.
  Layout();
  SchedulePaint();
}

void AssistantWebView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void AssistantWebView::OnDeepLinkReceived(const GURL& deep_link) {
  if (!assistant::util::IsWebDeepLink(deep_link))
    return;

  ReleaseWebContents();

  web_contents_id_token_ = base::UnguessableToken::Create();

  ash::mojom::ManagedWebContentsParamsPtr params(
      ash::mojom::ManagedWebContentsParams::New());
  params->url = assistant::util::GetWebUrl(deep_link).value();
  params->min_size_dip = gfx::Size(kPreferredWidthDip, kMaxHeightDip);
  params->max_size_dip = gfx::Size(kPreferredWidthDip, kMaxHeightDip);

  assistant_controller_->ManageWebContents(
      web_contents_id_token_.value(), std::move(params),
      base::BindOnce(&AssistantWebView::OnWebContentsReady,
                     web_contents_request_factory_.GetWeakPtr()));
}

void AssistantWebView::OnWebContentsReady(
    const base::Optional<base::UnguessableToken>& embed_token) {
  // TODO(dmblack): In the event that rendering fails, we should show a useful
  // message to the user (and perhaps close the UI).
  if (!embed_token.has_value())
    return;

  // When web contents are rendered in process, the WebView associated with the
  // returned |embed_token| is found in the AnswerCardContentsRegistry.
  if (app_list::AnswerCardContentsRegistry::Get()) {
    AddChildView(app_list::AnswerCardContentsRegistry::Get()->GetView(
        embed_token.value()));
  }

  // TODO(dmblack): Handle mash case.
}

void AssistantWebView::ReleaseWebContents() {
  web_contents_request_factory_.InvalidateWeakPtrs();

  if (!web_contents_id_token_.has_value())
    return;

  RemoveAllChildViews(/*delete_children=*/true);

  assistant_controller_->ReleaseWebContents(web_contents_id_token_.value());
  web_contents_id_token_.reset();
}

}  // namespace ash
