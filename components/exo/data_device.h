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
class FileHelper;
class Surface;

enum class DndAction { kNone, kCopy, kMove, kAsk };

class ScopedDataOffer;

// DataDevice to start drag and drop and copy and paste oprations.
class DataDevice : public WMHelper::DragDropObserver, public DataOfferObserver {
 public:
  explicit DataDevice(DataDeviceDelegate* delegate, FileHelper* file_helper);
  ~DataDevice() override;

  // Starts drag-and-drop operation.
  // |source| represents data comes from the client starting drag operation. Can
  // be null if the data will be transferred only in the client.  |origin| is
  // the surface which starts the drag and drop operation. |icon| is the
  // nullable image which is rendered at the next to cursor while drag
  // operation. |serial| is the unique number comes from input events which
  // triggers the drag and drop operation.
  void StartDrag(const DataSource* source,
                 Surface* origin,
                 Surface* icon,
                 uint32_t serial);

  // Sets selection data to the clipboard.
  // |source| represents data comes from the client. |serial| is the unique
  // number comes from input events which triggers the drag and drop operation.
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

  DataDeviceDelegate* const delegate_;
  FileHelper* const file_helper_;
  std::unique_ptr<ScopedDataOffer> data_offer_;

  DISALLOW_COPY_AND_ASSIGN(DataDevice);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_DEVICE_H_
