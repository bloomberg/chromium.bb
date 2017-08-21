// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/data_offer.h"

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "components/exo/data_offer_delegate.h"
#include "components/exo/data_offer_observer.h"
#include "components/exo/file_helper.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

class RefCountedString16 : public base::RefCountedMemory {
 public:
  static scoped_refptr<RefCountedString16> TakeString(
      base::string16&& to_destroy) {
    scoped_refptr<RefCountedString16> self(new RefCountedString16);
    to_destroy.swap(self->data_);
    return self;
  }

  // Overridden from base::RefCountedMemory:
  const unsigned char* front() const override {
    return reinterpret_cast<const unsigned char*>(data_.data());
  }
  size_t size() const override { return data_.size() * sizeof(base::char16); }

 protected:
  ~RefCountedString16() override {}

 private:
  base::string16 data_;
};

void WriteFileDescriptor(base::ScopedFD fd,
                         scoped_refptr<base::RefCountedMemory> memory) {
  if (!base::WriteFileDescriptor(fd.get(),
                                 reinterpret_cast<const char*>(memory->front()),
                                 memory->size()))
    DLOG(ERROR) << "Failed to write drop data";
}

}  // namespace

DataOffer::DataOffer(DataOfferDelegate* delegate) : delegate_(delegate) {}

DataOffer::~DataOffer() {
  delegate_->OnDataOfferDestroying(this);
  for (DataOfferObserver& observer : observers_) {
    observer.OnDataOfferDestroying(this);
  }
}

void DataOffer::AddObserver(DataOfferObserver* observer) {
  observers_.AddObserver(observer);
}

void DataOffer::RemoveObserver(DataOfferObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DataOffer::Accept(const std::string& mime_type) {
  NOTIMPLEMENTED();
}

void DataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  const auto it = drop_data_.find(mime_type);
  if (it == drop_data_.end()) {
    DLOG(ERROR) << "Unexpected mime type is requested";
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteFileDescriptor, std::move(fd), it->second));
}

void DataOffer::Finish() {
  NOTIMPLEMENTED();
}

void DataOffer::SetActions(const base::flat_set<DndAction>& dnd_actions,
                           DndAction preferred_action) {
  dnd_action_ = preferred_action;
  delegate_->OnAction(preferred_action);
}

void DataOffer::SetSourceActions(
    const base::flat_set<DndAction>& source_actions) {
  source_actions_ = source_actions;
  delegate_->OnSourceActions(source_actions);
}

void DataOffer::SetDropData(FileHelper* file_helper,
                            const ui::OSExchangeData& data) {
  DCHECK_EQ(0u, drop_data_.size());
  if (data.HasString()) {
    base::string16 string_content;
    if (data.GetString(&string_content)) {
      drop_data_.emplace(
          std::string(ui::Clipboard::kMimeTypeText),
          RefCountedString16::TakeString(std::move(string_content)));
    }
  }
  if (data.HasFile()) {
    base::FilePath path;
    if (data.GetFilename(&path)) {
      GURL url;
      if (file_helper->ConvertPathToUrl(path, &url)) {
        base::string16 url_string = base::UTF8ToUTF16(url.spec());
        drop_data_.emplace(
            file_helper->GetMimeTypeForUriList(),
            RefCountedString16::TakeString(std::move(url_string)));
      }
    }
  }
  for (const auto& pair : drop_data_) {
    delegate_->OnOffer(pair.first);
  }
}

}  // namespace exo
