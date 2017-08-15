// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_DEVICE_H_
#define COMPONENTS_EXO_DATA_DEVICE_H_

#include <cstdint>

#include "base/macros.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/wm_helper.h"

namespace ui {
class DropTargetEvent;
}

namespace exo {

class DataDeviceDelegate;
class DataOffer;
class DataSource;
class Surface;

enum class DndAction { kNone, kCopy, kMove, kAsk };

// Data transfer device providing access to inter-client data transfer
// mechanisms such as copy-and-paste and drag-and-drop.
class DataDevice : public WMHelper::DragDropObserver, public DataOfferObserver {
 public:
  explicit DataDevice(DataDeviceDelegate* delegate);
  ~DataDevice() override;

  // Starts drag-and-drop operation.
  // |source| is data source for the eventual transfer or null if data passing
  // is handled by a client internally. |origin| is a surface where the drag
  // originates. |icon| is drag-and-drop icon surface, which can be nullptr.
  // |serial| is a unique number of implicit grab.
  void StartDrag(const DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 uint32_t serial);

  // Copies data to the selection.
  // |source| is data source for the selection, or nullptr to unset the
  // selection. |serial| is a unique number of event which tigers SetSelection.
  void SetSelection(const DataSource* source, uint32_t serial);

  // Overridden from WMHelper::DragDropObserver:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  // Overridden from DataOfferObserver:
  void OnDataOfferDestroying(DataOffer* data_offer) override;

 private:
  Surface* GetEffectiveTargetForEvent(const ui::DropTargetEvent& event) const;
  void ClearDataOffer();

  DataDeviceDelegate* const delegate_;
  DataOffer* data_offer_;

  DISALLOW_COPY_AND_ASSIGN(DataDevice);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_DEVICE_H_
