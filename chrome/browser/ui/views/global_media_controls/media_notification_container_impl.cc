// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl.h"

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);

// The minimum number of enabled and visible user actions such that we should
// force the MediaNotificationView to be expanded.
constexpr int kMinVisibleActionsForExpanding = 4;

}  // anonymous namespace

MediaNotificationContainerImpl::MediaNotificationContainerImpl(
    MediaDialogView* parent,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : parent_(parent) {
  DCHECK(parent_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPreferredSize(kNormalSize);

  view_ = std::make_unique<media_message_center::MediaNotificationView>(
      this, std::move(item), nullptr, base::string16());
  view_->set_owned_by_client();
  ForceExpandedState();

  AddChildView(view_.get());
}

MediaNotificationContainerImpl::~MediaNotificationContainerImpl() = default;

void MediaNotificationContainerImpl::OnExpanded(bool expanded) {
  SetPreferredSize(expanded ? kExpandedSize : kNormalSize);
  PreferredSizeChanged();
  parent_->OnAnchorBoundsChanged();
}

void MediaNotificationContainerImpl::OnVisibleActionsChanged(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  has_many_actions_ = actions.size() >= kMinVisibleActionsForExpanding;
  ForceExpandedState();
}

void MediaNotificationContainerImpl::OnMediaArtworkChanged(
    const gfx::ImageSkia& image) {
  has_artwork_ = !image.isNull();
  ForceExpandedState();
}

void MediaNotificationContainerImpl::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}
