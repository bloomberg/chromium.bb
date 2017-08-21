// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_device.h"

#include "base/logging.h"
#include "components/exo/data_device_delegate.h"
#include "components/exo/data_offer.h"
#include "components/exo/surface.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"

namespace exo {

DataDevice::DataDevice(DataDeviceDelegate* delegate, FileHelper* file_helper)
    : delegate_(delegate), file_helper_(file_helper), data_offer_(nullptr) {
  WMHelper::GetInstance()->AddDragDropObserver(this);
}

DataDevice::~DataDevice() {
  delegate_->OnDataDeviceDestroying(this);
  WMHelper::GetInstance()->RemoveDragDropObserver(this);
  ClearDataOffer();
}

void DataDevice::StartDrag(const DataSource* source_resource,
                           Surface* origin_resource,
                           Surface* icon_resource,
                           uint32_t serial) {
  // TODO(hirono): Check if serial is valid. crbug.com/746111
  NOTIMPLEMENTED();
}

void DataDevice::SetSelection(const DataSource* source, uint32_t serial) {
  // TODO(hirono): Check if serial is valid. crbug.com/746111
  NOTIMPLEMENTED();
}

void DataDevice::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!data_offer_);

  Surface* surface = GetEffectiveTargetForEvent(event);
  if (!surface)
    return;

  base::flat_set<DndAction> dnd_actions;
  if (event.source_operations() & ui::DragDropTypes::DRAG_MOVE) {
    dnd_actions.insert(DndAction::kMove);
  }
  if (event.source_operations() & ui::DragDropTypes::DRAG_COPY) {
    dnd_actions.insert(DndAction::kCopy);
  }
  if (event.source_operations() & ui::DragDropTypes::DRAG_LINK) {
    dnd_actions.insert(DndAction::kAsk);
  }

  data_offer_ = delegate_->OnDataOffer();
  data_offer_->AddObserver(this);
  data_offer_->SetDropData(file_helper_, event.data());
  data_offer_->SetSourceActions(dnd_actions);
  data_offer_->SetActions(base::flat_set<DndAction>(), DndAction::kAsk);
  delegate_->OnEnter(surface, event.location_f(), *data_offer_);
}

int DataDevice::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (!data_offer_)
    return ui::DragDropTypes::DRAG_NONE;

  delegate_->OnMotion(event.time_stamp(), event.location_f());

  // TODO(hirono): dnd_action() here may not be updated. Chrome needs to provide
  // a way to update DND action asynchronously.
  switch (data_offer_->dnd_action()) {
    case DndAction::kMove:
      return ui::DragDropTypes::DRAG_MOVE;
    case DndAction::kCopy:
      return ui::DragDropTypes::DRAG_COPY;
    case DndAction::kAsk:
      return ui::DragDropTypes::DRAG_LINK;
    case DndAction::kNone:
      return ui::DragDropTypes::DRAG_NONE;
  }
}

void DataDevice::OnDragExited() {
  if (!data_offer_)
    return;

  delegate_->OnLeave();
  ClearDataOffer();
}

int DataDevice::OnPerformDrop(const ui::DropTargetEvent& event) {
  if (!data_offer_)
    return ui::DragDropTypes::DRAG_NONE;

  delegate_->OnDrop();
  ClearDataOffer();
  return ui::DragDropTypes::DRAG_NONE;
}

void DataDevice::OnDataOfferDestroying(DataOffer* data_offer) {
  if (data_offer_ == data_offer)
    ClearDataOffer();
}

Surface* DataDevice::GetEffectiveTargetForEvent(
    const ui::DropTargetEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(event.target());
  if (!window)
    return nullptr;
  Surface* target = Surface::AsSurface(window);
  if (!target)
    return nullptr;

  return delegate_->CanAcceptDataEventsForSurface(target) ? target : nullptr;
}

void DataDevice::ClearDataOffer() {
  if (!data_offer_)
    return;
  data_offer_->RemoveObserver(this);
  data_offer_ = nullptr;
}

}  // namespace exo
