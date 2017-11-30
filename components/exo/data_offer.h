// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_OFFER_H_
#define COMPONENTS_EXO_DATA_OFFER_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/base/class_property.h"

namespace base {
class RefCountedMemory;
}

namespace ui {
class Clipboard;
class OSExchangeData;
}

namespace exo {

class DataOfferDelegate;
class DataOfferObserver;
class FileHelper;
enum class DndAction;

// Object representing transferred data offered to a client.
class DataOffer final : public ui::PropertyHandler {
 public:
  explicit DataOffer(DataOfferDelegate* delegate);
  ~DataOffer();

  void AddObserver(DataOfferObserver* observer);
  void RemoveObserver(DataOfferObserver* observer);

  // Notifies to the DataOffer that the client can accept |mime type|.
  void Accept(const std::string& mime_type);

  // Notifies to the DataOffer that the client start receiving data of
  // |mime_type|. DataOffer writes the request data to |fd|.
  void Receive(const std::string& mime_type, base::ScopedFD fd);

  // Notifies to the DataOffer that the client no longer uses the DataOffer
  // object.
  void Finish();

  // Notifies to the DataOffer that possible and preferred drag and drop
  // operations selected by the client.
  void SetActions(const base::flat_set<DndAction>& dnd_actions,
                  DndAction preferred_action);

  // Sets the dropped data from |data| to the DataOffer object. |file_helper|
  // will be used to convert paths to handle mount points which is mounted in
  // the mount point namespace of clinet process.
  void SetDropData(FileHelper* file_helper, const ui::OSExchangeData& data);

  // Sets the clipboard data from |data| to the DataOffer object.
  void SetClipboardData(FileHelper* file_helper, const ui::Clipboard& data);

  // Sets the drag and drop actions which is offered by data source to the
  // DataOffer object.
  void SetSourceActions(const base::flat_set<DndAction>& source_actions);

  DndAction dnd_action() { return dnd_action_; }

 private:
  DataOfferDelegate* const delegate_;

  // Map between mime type and drop data bytes.
  base::flat_map<std::string, scoped_refptr<base::RefCountedMemory>> data_;
  base::flat_set<DndAction> source_actions_;
  DndAction dnd_action_;
  base::ObserverList<DataOfferObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DataOffer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_OFFER_H_
