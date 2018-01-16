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
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace exo {
namespace {

constexpr char kTextMimeTypeUtf8[] = "text/plain;charset=utf-8";
constexpr char kUriListSeparator[] = "\r\n";

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

void DataOffer::Accept(const std::string& mime_type) {}

void DataOffer::Receive(const std::string& mime_type, base::ScopedFD fd) {
  const auto it = data_.find(mime_type);
  if (it == data_.end()) {
    DLOG(ERROR) << "Unexpected mime type is requested";
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&WriteFileDescriptor, std::move(fd), it->second));
}

void DataOffer::Finish() {}

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
  DCHECK_EQ(0u, data_.size());
  if (data.HasString()) {
    base::string16 string_content;
    if (data.GetString(&string_content)) {
      data_.emplace(std::string(ui::Clipboard::kMimeTypeText),
                    RefCountedString16::TakeString(std::move(string_content)));
    }
  }
  if (data.HasFile()) {
    std::vector<ui::FileInfo> files;
    if (data.GetFilenames(&files)) {
      base::string16 url_list;
      for (const auto& info : files) {
        GURL url;
        // TODO(hirono): Need to fill the corret app_id.
        if (file_helper->GetUrlFromPath(/* app_id */ "", info.path, &url)) {
          if (!url_list.empty())
            url_list += base::UTF8ToUTF16(kUriListSeparator);
          url_list += base::UTF8ToUTF16(url.spec());
        }
      }
      data_.emplace(file_helper->GetMimeTypeForUriList(),
                    RefCountedString16::TakeString(std::move(url_list)));
    }
  }
  for (const auto& pair : data_) {
    delegate_->OnOffer(pair.first);
  }
}

void DataOffer::SetClipboardData(FileHelper* file_helper,
                                 const ui::Clipboard& data) {
  DCHECK_EQ(0u, data_.size());
  if (data.IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                             ui::CLIPBOARD_TYPE_COPY_PASTE)) {
    base::string16 content;
    data.ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &content);
    std::string utf8_content = base::UTF16ToUTF8(content);
    data_.emplace(std::string(kTextMimeTypeUtf8),
                  base::RefCountedString::TakeString(&utf8_content));
  }
  for (const auto& pair : data_) {
    delegate_->OnOffer(pair.first);
  }
}

}  // namespace exo
